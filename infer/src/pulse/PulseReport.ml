(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

open! IStd
module F = Format
module L = Logging
open PulseBasicInterface
open PulseDomainInterface

type reporting_scope =
  { specialization: Specialization.Pulse.t option
  ; mutable pending: ExecutionDomain.conditional_manifest_report list
  ; mutable exposed: bool }

(* Registered with the on-demand state manager: nested analyses must neither inherit nor
   overwrite their caller's pending reports and specialization. *)
let reporting_scope = AnalysisGlobalState.make_dls ~init:(fun () -> None)

let register_conditional report =
  Option.iter (DLS.get reporting_scope) ~f:(fun scope -> scope.pending <- report :: scope.pending)


let current_specialization () =
  Option.bind (DLS.get reporting_scope) ~f:(fun scope -> scope.specialization)


let has_exposed_context () = Option.exists (DLS.get reporting_scope) ~f:(fun scope -> scope.exposed)

let mark_incomplete () =
  Option.iter (DLS.get reporting_scope) ~f:(fun scope -> scope.exposed <- true)


(* Is nullptr dereference issue in Java class annotated with [@Nullsafe] *)
let is_nullptr_dereference_in_nullsafe_class tenv ~is_nullptr_dereference jn =
  is_nullptr_dereference
  && match NullsafeMode.of_java_procname tenv jn with Default -> false | Local | Strict -> true


(* Which allocation a leak is about: the chain of calls that leads to it, down to the location
   that allocates. The outer location alone is not enough -- two different allocations inside one
   helper share the call that reaches them, and keying on it merges two findings into one.

   The chain is kept as the calls themselves rather than as a copy of the trace. A trace is a nest
   of records, so copying one costs an allocation and a stack frame per level of the call chain,
   and comparing two of them walks both nests.

   The histories are left out because they describe how a value was reached rather than which
   allocation it came from, and the several reports of one allocation must still collapse into
   one. No test here needs them: the allocating [Immediate] is built with an empty history, and
   the [ViaCall] history is the caller's history for the value at the call, the same on every path
   that reports the leak. They are left out so that an identity test cannot come to depend on
   them. *)
type allocation_site = {calls: (CallEvent.t * Location.t) list; allocation: Location.t}

(* The allocating location first: it tells most pairs of allocations apart in one comparison, so
   the chains are walked only for the ones it does not. The walk is tail-recursive, [&&] running
   its right-hand side in tail position, so a deep chain costs no stack. *)
let equal_allocation_site {calls; allocation} {calls= calls'; allocation= allocation'} =
  let rec equal_calls calls calls' =
    match (calls, calls') with
    | [], [] ->
        true
    | (f, location) :: calls, (f', location') :: calls' ->
        CallEvent.equal f f' && Location.equal location location' && equal_calls calls calls'
    | [], _ :: _ | _ :: _, [] ->
        false
  in
  Location.equal allocation allocation' && equal_calls calls calls'


(* Tail-recursive, so that how deep a trace may be is bounded by memory and not by the stack. The
   calls come out outermost first, the order they are made in, which is only so that the key reads
   like the trace it comes from: equality does not depend on that order, only on it being the same
   on both sides. *)
let allocation_site_of_trace (trace : Trace.t) : allocation_site =
  let rec walk calls (trace : Trace.t) =
    match trace with
    | Immediate {location} ->
        {calls= List.rev calls; allocation= location}
    | ViaCall {f; location; in_call} ->
        walk ((f, location) :: calls) in_call
  in
  walk [] trace


(* What makes two leak reports the same finding: the same kind of issue, about the same kind
   of resource, at the same allocation. *)
type leak_key = IssueType.t * Diagnostic.resource * allocation_site

let equal_leak_key (issue_type, resource, allocation_site) (issue_type', resource', allocation_site')
    =
  IssueType.equal issue_type issue_type'
  && Diagnostic.equal_resource resource resource'
  && equal_allocation_site allocation_site allocation_site'


(* A leaked allocation is one bug however many ways the procedure leaves it behind: the same
   allocation becomes unreachable once per path out of the procedure, and each of those used to
   be logged separately, so a resource used before it is dropped was reported both where it was
   last touched and again at the end of the method. Remember the allocations already reported in
   the procedure being analysed and log only the first. This affects logging alone: the state
   [report_summary_error] derives from the diagnostic is computed as before, so no path and no
   other issue changes. *)
(* The identity of a leaked allocation, written out, so that the report writer can tell two
   entries that are one finding from two that are not.

   It is needed because de-duplication inside the analysis cannot see far enough. One procedure is
   analysed once per specialization, each analysis starting with its own empty memory, so the same
   leak can be described by several of them -- at a different way out each time, because each sees
   a different set of paths. Those descriptions reach the error log as separate entries, and the
   log keys an entry on its message, so it cannot tell they are the same finding. The key travels
   with the entry and lets the report writer collapse them.

   It says exactly what [equal_leak_key] compares and no less: anything coarser would merge two
   findings into one. The constructors are matched by hand rather than printed through some
   generic printer so that adding one is a type error here rather than a silent merge.

   The parts are kept apart as a list, never joined into one string. A java call event prints its
   argument types separated by commas, so a key built by joining the chain of calls on some
   separator could be spelled by a different chain that happens to contain it, and two findings
   would become one. A list has nowhere for that to happen: the parts are compared one against
   one, and two chains of different length differ in length. *)
let dedup_key_of_leak ((issue_type, resource, {calls; allocation}) : leak_key) =
  let pp_resource fmt (resource : Diagnostic.resource) =
    match resource with
    | CSharpClass name ->
        F.fprintf fmt "csharp:%a" CSharpClassName.pp name
    | JavaClass name ->
        F.fprintf fmt "java:%a" JavaClassName.pp name
    | Awaitable ->
        F.pp_print_string fmt "awaitable"
    | HackBuilderResource name ->
        F.fprintf fmt "hack:%a" HackClassName.pp name
    | Memory allocator ->
        F.fprintf fmt "memory:%a" Attribute.pp_allocator allocator
  in
  let pp_location fmt ({file; line; col} : Location.t) =
    F.fprintf fmt "%a:%d:%d" SourceFile.pp file line col
  in
  let location location = F.asprintf "%a" pp_location location in
  (* A call is named by what kind of thing it is as well as by which one. Printing alone does not
     tell the kinds apart: a model and a procedure taking its name both come out as "`x`
     (modelled)", so a model named after something a procedure name spells would be the same call
     here and two findings would become one. The name is the procedure's unique id rather than its
     printed form, for the same reason. *)
  let call_kind_and_name (call : CallEvent.t) =
    match call with
    | Call proc_name ->
        ("call", Procname.to_unique_id proc_name)
    | Model model ->
        ("model", model)
    | ModelName proc_name ->
        ("model-name", Procname.to_unique_id proc_name)
    | SkippedKnownCall (proc_name, NoSummary) ->
        ("skipped-no-summary", Procname.to_unique_id proc_name)
    | SkippedKnownCall (proc_name, ArityMismatch) ->
        ("skipped-arity-mismatch", Procname.to_unique_id proc_name)
    | SkippedKnownCall (proc_name, SummaryNotApplicable) ->
        ("skipped-summary-not-applicable", Procname.to_unique_id proc_name)
    | SkippedKnownCall (proc_name, ForceContinue) ->
        ("skipped-force-continue", Procname.to_unique_id proc_name)
    | SkippedUnknownCall call_exp ->
        ("unknown", F.asprintf "%a" Exp.pp call_exp)
  in
  issue_type.IssueType.unique_id
  :: F.asprintf "%a" pp_resource resource
  :: List.concat_map calls ~f:(fun (call, call_location) ->
      let kind, name = call_kind_and_name call in
      [kind; name; location call_location] )
  @ [location allocation]


let reported_leak_allocations : leak_key list ref DLS.key =
  AnalysisGlobalState.make_dls ~init:(fun () -> ref [])


let leak_allocation ~latent (diagnostic : Diagnostic.t) =
  match diagnostic with
  | ResourceLeak {resource; allocation_trace} ->
      Some
        ( Diagnostic.get_issue_type ~latent diagnostic
        , resource
        , allocation_site_of_trace allocation_trace )
  | _ ->
      None


let is_already_reported_leak leak =
  DLS.get reported_leak_allocations |> ( ! ) |> List.exists ~f:(equal_leak_key leak)


(* Only an allocation whose report really reached the error log counts as reported. The log keys
   an entry on its message, so two allocations that read the same become one entry, and an issue
   can also be filtered out or suppressed on its way there. Remembering an allocation whose report
   went nowhere would make the next report of it -- which may well read differently, and so be an
   entry of its own -- look like a repeat, and the leak would not be reported at all. *)
let remember_reported_leak leak =
  let reported = DLS.get reported_leak_allocations in
  reported := leak :: !reported


(* A leak report is held back until the procedure has been analysed, instead of being logged where
   it is produced. The several exits that leak one allocation are summarised one after another and
   only one of them is reported, and logging as we go means reporting whichever the analysis
   reached first -- an order that comes from how the disjuncts happen to be laid out, not from
   anything in the source. The location decides where the warning appears, so it also decides
   where a suppression comment has to go for the reader: on the exit the order happened to pick,
   their comment works, and on any other it silently does nothing. Collect the candidates instead
   and, once the procedure is done, report the one at the EARLIEST exit. *)
type deferred_leak =
  { leak: leak_key
  ; location: Location.t
  ; issue_type: IssueType.t
  ; message: string
  ; node: Procdesc.Node.t option
  ; emit: unit -> bool }

let deferred_leaks : deferred_leak list ref DLS.key =
  AnalysisGlobalState.make_dls ~init:(fun () -> ref [])


(* Earliest by file, then line, then column: [Location] compares in that order, so the choice
   depends only on where the exits are written and not on the order they were analysed in. *)
let compare_by_location {location= l1} {location= l2} = Location.compare l1 l2

(* One candidate is held per allocation and message, not one per way out.

   A procedure can leave an allocation behind on as many paths as it has disjuncts, and holding
   every one of them would make what is retained grow with the analysis rather than with the
   procedure's text -- each candidate keeps a message, a trace and the closure that files them.
   Dropping all but one of the candidates that would land on the same entry is not a choice about
   which report to make, and it is worth being exact about why. The error log keys an entry on
   three things: the severity, the issue type and the description. The severity is the issue
   type's own, nothing here passing an override. The description is built from the message AND a
   suggestion -- but the equality the log uses compares neither the suggestion nor anything else,
   only the descriptions, which for a verbatim description are the message alone. And even were
   the suggestion compared, a leak's suggestion is a function of its resource, and the resource is
   part of the key these candidates already share. So two candidates of one allocation with the
   same issue type and message land on the same entry: either both reach the log or neither does.
   The later one is kept only to be tried after the earlier fails, and it would fail in the same
   way, so keeping the earlier alone reports exactly what keeping both would.

   That rests on two independent facts and it takes BOTH of them failing to break it: a
   description that compared more than the message, AND something in that extra part that can
   differ between two ways out of one allocation -- a suggestion that depended on the path rather
   than on the resource, say. A candidate dropped here could then have been recorded where the one
   kept was not, and the finding would be lost.

   What survives is one candidate per distinct message, so the number held is bounded by how many
   different things the procedure can say about one allocation, not by how many paths it has. *)
let defer_leak deferred =
  let deferred_leaks = DLS.get deferred_leaks in
  (* The issue type as it will be logged, not the one in the key: they agree for a leak, but it
     is what the log compares that decides whether two candidates are interchangeable. *)
  let lands_on_the_same_entry other =
    equal_leak_key deferred.leak other.leak
    && IssueType.equal deferred.issue_type other.issue_type
    && String.equal deferred.message other.message
  in
  (* Tail-recursive, and [rev_append] puts the prefix back in order. *)
  let rec replace prefix = function
    | [] ->
        None
    | other :: rest when lands_on_the_same_entry other ->
        (* Strictly earlier, so that candidates at one location keep the order they arrived in,
           which is what the stable sort below would have done with them. *)
        let keep =
          if Location.compare deferred.location other.location < 0 then deferred else other
        in
        Some (List.rev_append prefix (keep :: rest))
    | other :: rest ->
        replace (other :: prefix) rest
  in
  match replace [] !deferred_leaks with
  | Some candidates ->
      deferred_leaks := candidates
  | None ->
      deferred_leaks := deferred :: !deferred_leaks


let flush_deferred_leaks () =
  let deferred = DLS.get deferred_leaks in
  let candidates = List.rev !deferred in
  deferred := [] ;
  (* Same allocation, several exits: a group is one finding, its members are the places that
     finding could be reported at. [Diagnostic.resource] has no comparison, only equality, so the
     grouping is by equality rather than by sorting on the key. The groups are few and small. *)
  let rec group_by_allocation groups = function
    | [] ->
        List.rev groups
    | ({leak} as deferred) :: rest ->
        let same, others = List.partition_tf rest ~f:(fun d -> equal_leak_key leak d.leak) in
        let group = List.stable_sort (deferred :: same) ~compare:compare_by_location in
        group_by_allocation (group :: groups) others
  in
  (* The groups are ordered by where they will be reported, so that two findings whose messages
     turn out to read the same are resolved the same way however the analysis reached them. *)
  group_by_allocation [] candidates
  |> List.stable_sort ~compare:(fun g1 g2 ->
      match (g1, g2) with d1 :: _, d2 :: _ -> compare_by_location d1 d2 | _ -> 0 )
  |> List.iter ~f:(fun group ->
      match group with
      | [] ->
          ()
      | {leak} :: _ when is_already_reported_leak leak ->
          (* Reported by an earlier analysis of this procedure, for another specialization. *)
          L.d_printfln "Not reporting this leak again: its allocation is already reported here"
      | _ :: _ ->
          (* The earliest exit first, and the next one if that report does not reach the log:
                it can be filtered out, or read exactly like an entry that is already there, and
                a finding must not be lost because the place we chose to report it was taken. *)
          let saved_node = AnalysisState.get_node () in
          Fun.protect
            ~finally:(fun () -> AnalysisState.set_node_opt saved_node)
            (fun () ->
              List.find group ~f:(fun {node; emit} ->
                  (* [Reporting] stamps an issue with the "last node seen", which by now is the
                        end of the procedure rather than the exit this report is about. *)
                  AnalysisState.set_node_opt node ;
                  emit () )
              |> Option.iter ~f:(fun {leak} -> remember_reported_leak leak) ) )


let do_report {InterproceduralAnalysis.tenv; proc_desc; err_log} ~is_suppressed ~latent
    ?location_override ?extra_trace ?reachable_from (diagnostic : Diagnostic.t) =
  let open Diagnostic in
  let is_hidden = is_suppressed && not Config.pulse_report_issues_for_tests in
  (* Which allocation a leak is about is asked only where the answer can be used: a hidden
     diagnostic is dropped on the next line, and reading its chain of calls would be work done
     for nobody. The allocations already reported are not consulted here any more: a leak is held
     back to the end of the procedure, so that question is asked of the whole group, in
     [flush_deferred_leaks]. *)
  let leak = if is_hidden then None else leak_allocation ~latent diagnostic in
  if is_hidden then ()
  else
    (* Report suppressed issues with a message to distinguish them from non-suppressed issues.
       Useful for infer's tests. *)
    let loc_instantiated = Diagnostic.get_location_instantiated diagnostic in
    let suppressed_extra_trace =
      if is_suppressed && Config.pulse_report_issues_for_tests then
        let depth = 0 in
        let tags = [] in
        let location = Diagnostic.get_location diagnostic in
        [Errlog.make_trace_element depth location "*** SUPPRESSED ***" tags]
      else []
    in
    let extras =
      let may_depend_on_an_unknown_value =
        match diagnostic with
        | AccessToInvalidAddress {may_depend_on_an_unknown_value= true} ->
            let elt : Jsonbug_t.may_depend_on_an_unknown_value = {value= true} in
            Some elt
        | _ ->
            None
      in
      let transitive_callees, transitive_missed_captures =
        let to_jsonbug_missed_capture class_name =
          {Jsonbug_t.class_name= Typ.Name.name class_name}
        in
        match diagnostic with
        | TransitiveAccess {transitive_callees; transitive_missed_captures} ->
            ( TransitiveInfo.Callees.to_jsonbug_transitive_callees transitive_callees
            , Typ.Name.Set.elements transitive_missed_captures
              |> List.map ~f:to_jsonbug_missed_capture )
        | _ ->
            ([], [])
      in
      let copy_type = get_copy_type diagnostic |> Option.map ~f:Typ.to_string in
      let taint_source, taint_sink =
        let proc_name_of_taint taint_item =
          Format.asprintf "%a" TaintItem.pp_value_plain (TaintItem.value_of_taint taint_item)
        in
        match diagnostic with
        | TaintFlow {flow_kind= FlowFromSource; source= source, _} ->
            (Some (proc_name_of_taint source), None)
        | TaintFlow {flow_kind= FlowToSink; sink= sink, _} ->
            (None, Some (proc_name_of_taint sink))
        | TaintFlow {flow_kind= TaintedFlow; source= source, _; sink= sink, _} ->
            (Some (proc_name_of_taint source), Some (proc_name_of_taint sink))
        | _ ->
            (None, None)
      in
      let tainted_expression =
        match diagnostic with
        | TaintFlow {expr} ->
            Some (F.asprintf "%a" DecompilerExpr.pp expr)
        | _ ->
            None
      in
      let taint_policy_privacy_effect =
        match diagnostic with
        | TaintFlow {flow_kind= TaintedFlow; policy_privacy_effect; _} ->
            policy_privacy_effect
        | _ ->
            None
      in
      let taint_report_as_issue_type, taint_report_as_category =
        match diagnostic with
        | TaintFlow {report_as_issue_type; report_as_category} ->
            (report_as_issue_type, report_as_category)
        | _ ->
            (None, None)
      in
      let taint_extra : Jsonbug_t.taint_extra option =
        match
          ( taint_source
          , taint_sink
          , taint_policy_privacy_effect
          , tainted_expression
          , taint_report_as_issue_type
          , taint_report_as_category )
        with
        | None, None, None, None, None, None ->
            None
        | _, _, _, _, _, _ ->
            Some
              { taint_source
              ; taint_sink
              ; taint_policy_privacy_effect
              ; tainted_expression
              ; report_as_issue_type= taint_report_as_issue_type
              ; report_as_category= taint_report_as_category }
      in
      let config_usage_extra : Jsonbug_t.config_usage_extra option =
        match diagnostic with
        | ConfigUsage {pname; config; branch_location= {file; line}} ->
            Some
              { config_name= F.asprintf "%a" ConfigName.pp config
              ; function_name= Procname.to_string pname
              ; filename= SourceFile.to_string ~force_relative:true file
              ; line_number= line }
        | _ ->
            None
      in
      { Jsonbug_t.cost_polynomial= None
      ; cost_degree= None
      ; copy_type
      ; config_usage_extra
      ; may_depend_on_an_unknown_value
      ; reachable_from
      ; taint_extra
      ; racerd_access= None
      ; transitive_callees
      ; transitive_missed_captures }
    in
    (* Remap to a different issue type for nullptr derefs in @Nullsafe Java classes,
       and for Swift unwraps of a [.none] Optional (-> SWIFT_NPE). *)
    let get_issue_type tenv ~latent diagnostic proc_desc =
      let original_issue_type = Diagnostic.get_issue_type diagnostic ~latent in
      if IssueType.equal original_issue_type (IssueType.nullptr_dereference ~latent) then
        match Procdesc.get_proc_name proc_desc with
        | Procname.Java jn
          when is_nullptr_dereference_in_nullsafe_class tenv ~is_nullptr_dereference:true jn
               && Config.pulse_nullsafe_report_npe_as_separate_issue_type ->
            IssueType.nullptr_dereference_in_nullsafe_class ~latent
        | _ ->
            original_issue_type
      else if IssueType.equal original_issue_type (IssueType.optional_empty_access ~latent) then
        match Procdesc.get_proc_name proc_desc with
        | Procname.Swift _ ->
            IssueType.swift_npe ~latent
        | _ ->
            original_issue_type
      else original_issue_type
    in
    let issue_type = get_issue_type tenv ~latent diagnostic proc_desc in
    let message, suggestion = get_message_and_suggestion diagnostic in
    let message =
      Option.fold reachable_from ~init:message ~f:(fun message reachable_from ->
          F.sprintf "(reachable from %s) %s" reachable_from message )
    in
    let autofix = PulseAutofix.get_autofix proc_desc diagnostic in
    let loc =
      match location_override with
      | Some location ->
          location
      | None ->
          Diagnostic.get_location diagnostic
    in
    let ltr =
      Option.fold extra_trace
        ~init:(suppressed_extra_trace @ get_trace diagnostic)
        ~f:(fun ltr extra_trace ->
          Trace.add_to_errlog ~nesting:0
            ~pp_immediate:(fun fmt -> F.fprintf fmt "original issue trace starts here")
            extra_trace ltr )
    in
    let dedup_key = Option.map leak ~f:dedup_key_of_leak in
    let emit () =
      L.d_printfln ~color:Red "Reporting issue: %a: %s" IssueType.pp issue_type message ;
      Reporting.log_issue_recorded proc_desc err_log ~loc ?loc_instantiated ~ltr ~extras ~autofix
        ?suggestion ?dedup_key Pulse issue_type message
    in
    match leak with
    | Some leak ->
        L.d_printfln "Holding this leak back until the procedure is done: %a at %a" IssueType.pp
          issue_type Location.pp loc ;
        defer_leak {leak; location= loc; issue_type; message; node= AnalysisState.get_node (); emit}
    | None ->
        emit () |> ignore


let report analysis_data ~is_suppressed ~latent diagnostic =
  do_report analysis_data ~is_suppressed ~latent diagnostic


(** The node of [proc_desc] that [loc] belongs to, lowest id first so that the choice does not
    depend on the order nodes are stored in. [None] when the location is not in this procedure,
    which leaves the report with no node rather than someone else's. *)
let node_at_location proc_desc (loc : Location.t) =
  (* Match on file and line rather than on the whole location: a node's own location can differ
     from the diagnostic's in column and in the macro-expansion fields. *)
  let same_line (node_loc : Location.t) =
    Int.equal node_loc.line loc.line && SourceFile.equal node_loc.file loc.file
  in
  Procdesc.fold_nodes proc_desc ~init:None ~f:(fun acc node ->
      if same_line (Procdesc.Node.get_loc node) then
        match acc with
        | Some best when (Procdesc.Node.get_id best :> int) <= (Procdesc.Node.get_id node :> int) ->
            acc
        | _ ->
            Some node
      else acc )


let report_conditional_origin analysis_data
    {ExecutionDomain.origin= {proc_name}; original_diagnostic; is_suppressed} =
  match Procdesc.load proc_name with
  | Some proc_desc ->
      let err_log = Errlog.empty () in
      let origin_data = InterproceduralAnalysis.for_procedure proc_desc err_log analysis_data in
      (* [Reporting] stamps an issue with [AnalysisState]'s "last node seen". Here that node
         belongs to the procedure this worker is analysing -- a CALLER of [proc_name] -- and not
         to [proc_name], whose errlog the issue is being filed into. Inheriting it records a node
         from somebody else's CFG, and which caller reaches this first is decided by the
         scheduler: under the default restart scheduler a procedure reached from two callers gets
         a different node, and so a different node_key, from run to run. Pin the node to the
         origin procedure instead, so it depends only on the issue and not on who got here. *)
      let saved_node = AnalysisState.get_node () in
      AnalysisState.set_node_opt
        (node_at_location proc_desc (Diagnostic.get_location original_diagnostic)) ;
      Fun.protect
        ~finally:(fun () -> AnalysisState.set_node_opt saved_node)
        (fun () -> report origin_data ~is_suppressed ~latent:false original_diagnostic) ;
      analysis_data.InterproceduralAnalysis.add_errlog proc_name err_log
  | None ->
      (* A missing origin must not turn a pending error into a silent path. *)
      report analysis_data ~is_suppressed ~latent:false original_diagnostic


let with_specialization analysis_data specialization ~f =
  let saved = DLS.get reporting_scope in
  let scope = {specialization; pending= []; exposed= false} in
  DLS.set reporting_scope (Some scope) ;
  Fun.protect
    ~finally:(fun () -> DLS.set reporting_scope saved)
    (fun () ->
      match f () with
      | result ->
          if scope.exposed then List.iter scope.pending ~f:(report_conditional_origin analysis_data) ;
          flush_deferred_leaks () ;
          result
      | exception exn ->
          (* Same condition as the completed path. An analysis can be abandoned for a reason that
             has nothing to do with this procedure -- the scheduler raises
             [RestartSchedulerException.ProcnameAlreadyLocked] when another worker has taken the
             procedure, and the analysis is simply redone -- and reporting the pending issues of a
             scope that never finished makes the result depend on which worker got there first.
             What the finished analysis would not have reported, an abandoned one must not report
             either. *)
          let backtrace = Stdlib.Printexc.get_raw_backtrace () in
          if scope.exposed then List.iter scope.pending ~f:(report_conditional_origin analysis_data) ;
          (* An abandoned analysis must not report what a finished one would not have; the leaks
             held back by a scope that never completed are dropped with it. *)
          DLS.get deferred_leaks := [] ;
          Stdlib.Printexc.raise_with_backtrace exn backtrace )


let callable_class tenv (name : Typ.Name.t) =
  match name with
  | Typ.CFunction _ ->
      true
  | Typ.CppClass _ ->
      let spelling = Typ.Name.name name in
      String.is_prefix spelling ~prefix:"std::function<"
      || String.is_prefix spelling ~prefix:"folly::Function<"
      || Option.exists (Tenv.lookup tenv name) ~f:(fun {Struct.methods} ->
          List.exists methods ~f:(fun {Struct.name} -> Procname.is_cpp_call_operator name) )
  | _ ->
      false


let rec callable_type tenv typ =
  match typ.Typ.desc with
  | Tptr (typ, _) ->
      callable_type tenv typ
  | Tfun _ ->
      true
  | Tstruct name ->
      callable_class tenv name
  | _ ->
      false


let note_unknown_actuals tenv actuals astate =
  let rec callable_value depth value =
    Option.exists (PulseArithmetic.get_dynamic_type value astate) ~f:(fun {Formula.typ} ->
        callable_type tenv typ )
    || depth > 0
       && Option.exists (Memory.find_edge_opt value Dereference astate) ~f:(fun (value, _) ->
           callable_value (depth - 1) value )
  in
  Option.iter (DLS.get reporting_scope) ~f:(fun scope ->
      if
        List.exists actuals ~f:(fun ((value, _), typ) ->
            callable_type tenv typ || callable_value 2 value )
      then scope.exposed <- true )


let has_trivial_callable_actual tenv actuals astate =
  let rec trivial depth value =
    Option.exists (PulseArithmetic.get_dynamic_type value astate) ~f:(fun {Formula.typ} ->
        match typ.Typ.desc with
        | Tstruct name when callable_class tenv name ->
            Option.exists (Tenv.lookup tenv name) ~f:(fun {Struct.class_info} ->
                match class_info with
                | Struct.ClassInfo.CppClassInfo {is_trivially_copyable} ->
                    is_trivially_copyable
                | _ ->
                    false )
        | _ ->
            false )
    || depth > 0
       && Option.exists (Memory.find_edge_opt value Dereference astate) ~f:(fun (value, _) ->
           trivial (depth - 1) value )
  in
  List.exists actuals ~f:(fun ((value, _), _) -> trivial 2 value)


let note_summary_escapes {InterproceduralAnalysis.tenv; proc_desc}
    (summary : AbductiveDomain.Summary.t) =
  Option.iter (DLS.get reporting_scope) ~f:(fun scope ->
      if not (List.is_empty scope.pending) then
        let post = AbductiveDomain.Summary.get_post summary in
        let formal_names =
          Procdesc.get_formals proc_desc |> List.map ~f:fst3 |> Mangled.Set.of_list
        in
        let rec reachable_callable seen written value =
          if AbstractValue.Set.mem value seen then false
          else
            let seen = AbstractValue.Set.add value seen in
            let written =
              written || Option.is_some (UnsafeAttributes.get_written_to value post.attrs)
            in
            written
            && Option.exists
                 (PulseArithmetic.get_dynamic_type value (summary :> AbductiveDomain.t))
                 ~f:(fun {Formula.typ} -> callable_type tenv typ)
            || Option.exists (UnsafeMemory.find_opt value post.heap) ~f:(fun edges ->
                UnsafeMemory.Edges.exists edges ~f:(fun (_, (value, _)) ->
                    reachable_callable seen written value ) )
        in
        if
          UnsafeStack.exists
            (fun var origin ->
              Option.exists (Var.get_pvar var) ~f:(fun pvar ->
                  let exported = Pvar.is_global pvar || Pvar.is_return pvar in
                  (exported || Mangled.Set.mem (Pvar.get_name pvar) formal_names)
                  && reachable_callable AbstractValue.Set.empty exported (ValueOrigin.value origin) ) )
            post.stack
        then scope.exposed <- true )


let depends_on_requested_callable specialization = function
  | Diagnostic.AccessToInvalidAddress {invalidation= ConstantDereference constant; access_trace}
    when IntLit.iszero constant ->
      Trace.exists_call access_trace ~f:(function
        | Call pname when Procname.is_cpp_call_operator pname ->
            Option.exists
              (Procname.get_class_type_name pname)
              ~f:(Specialization.Pulse.has_type_in_specialization specialization)
        | _ ->
            false )
  | _ ->
      false


let report_if_entry_point ({InterproceduralAnalysis.proc_desc} as analysis_data) trace_to_error
    diagnostic =
  let proc_name_s = Procdesc.get_proc_name proc_desc |> Procname.to_string in
  if
    List.exists Config.pulse_report_issues_reachable_from ~f:(fun regex ->
        Str.string_match regex proc_name_s 0 )
  then
    let location_override = Trace.get_outer_location trace_to_error in
    do_report analysis_data ~is_suppressed:false ~latent:false ~location_override
      ~extra_trace:trace_to_error ~reachable_from:proc_name_s diagnostic


let report_latent_issue analysis_data latent_issue ~is_suppressed =
  LatentIssue.to_diagnostic latent_issue |> report analysis_data ~latent:true ~is_suppressed


(* skip reporting for constant dereference (eg null dereference) if the source of the null value is
   not on the path of the access, otherwise the report will probably be too confusing: the actual
   source of the null value can be obscured as any value equal to 0 (or the constant) can be
   selected as the candidate for the trace, even if it has nothing to do with the error besides
   being equal to the value being dereferenced *)
let is_constant_deref_without_invalidation (invalidation : Invalidation.t) access_trace =
  let res =
    match invalidation with
    | ConstantDereference _ | ComparedToNullInThisProcedure _ ->
        not
          (Trace.exists access_trace ~f:(function
            | Invalidated (trace_invalidation, _, _)
              when Invalidation.is_same_type trace_invalidation invalidation ->
                true
            | _ ->
                false ))
    | CFree
    | CppDelete
    | CppDeleteArray
    | EndIterator
    | FClose _
    | GoneOutOfScope _
    | HandedOverToStream _
    | OptionalEmpty
    | OutOfBounds _
    | StdVector _
    | StdString _
    | CppMap _
    | StdContainer _
    | CppDestructedInPlace ->
        false
  in
  if res then
    L.d_printfln "no invalidation in acces trace %a"
      (Trace.pp ~pp_immediate:(fun fmt -> F.fprintf fmt "immediate"))
      access_trace ;
  res


let is_constant_deref_without_invalidation_diagnostic (diagnostic : Diagnostic.t) =
  match diagnostic with
  | AssertionError _
  | ConfigUsage _
  | ConstRefableParameter _
  | DynamicTypeMismatch _
  | ErlangError _
  | FatalOnNullableReturn _
  | InfiniteLoopError _
  | HackCannotInstantiateAbstractClass _
  | MissingNullabilityAnnotation _
  | MutualRecursionCycle _
  | ReadonlySharedPtrParameter _
  | ReadUninitialized _
  | ResourceLeak _
  | RetainCycle _
  | StackVariableAddressEscape _
  | TaintFlow _
  | TransitiveAccess _
  | UninitMethod _
  | UnnecessaryCopy _ ->
      false
  | AccessToInvalidAddress {invalidation; access_trace} ->
      is_constant_deref_without_invalidation invalidation access_trace


let is_optional_empty diagnostic =
  match (diagnostic : Diagnostic.t) with
  | AccessToInvalidAddress {invalidation= OptionalEmpty} ->
      true
  | _ ->
      false


(* Skip reporting nullptr dereferences in Java classes annotated with [@Nullsafe] if requested *)
let should_skip_reporting_nullptr_dereference_in_nullsafe_class tenv ~is_nullptr_dereference jn =
  (not Config.pulse_nullsafe_report_npe)
  && is_nullptr_dereference_in_nullsafe_class tenv ~is_nullptr_dereference jn


let is_suppressed tenv proc_desc ~is_nullptr_dereference ~is_constant_deref_without_invalidation
    ~is_optional_empty =
  if is_constant_deref_without_invalidation then (
    L.d_printfln ~color:Red
      "Dropping error: constant dereference with no invalidation in the access trace" ;
    true )
  else
    match Procdesc.get_proc_name proc_desc with
    | Procname.Java jn when is_nullptr_dereference ->
        should_skip_reporting_nullptr_dereference_in_nullsafe_class tenv ~is_nullptr_dereference jn
    | pname ->
        Procname.is_cpp_lambda pname && is_optional_empty


let summary_of_error_post proc_desc location mk_error astate =
  match AbductiveDomain.Summary.of_post (Procdesc.get_attributes proc_desc) location astate with
  | Sat (Ok summary)
  | Sat
      ( Error (`MemoryLeak (summary, _, _, _, _))
      | Error (`JavaResourceLeak (summary, _, _, _, _))
      | Error (`UnawaitedAwaitable (summary, _, _, _))
      | Error (`HackUnfinishedBuilder (summary, _, _, _, _))
      | Error (`CSharpResourceLeak (summary, _, _, _, _)) ) ->
      (* ignore potential memory leaks: error'ing in the middle of a function will typically produce
         spurious leaks *)
      Sat (mk_error summary)
  | Sat (Error (`PotentialInvalidAccessSummary (summary, astate, addr, trace))) ->
      (* ignore the error we wanted to report (with [mk_error]): the abstract state contained a
         potential error already so report [error] instead *)
      Sat
        (AccessResult.of_abductive_summary_error
           (`PotentialInvalidAccessSummary (summary, astate, addr, trace)) )
  | Unsat _ as unsat ->
      unsat


let summary_error_of_error proc_desc location (error : AccessResult.error) : _ SatUnsat.t =
  match error with
  | WithSummary (error, summary) ->
      Sat (error, summary)
  | PotentialInvalidAccess {astate}
  | PotentialInvalidSpecializedCall {astate}
  | ReportableError {astate} ->
      summary_of_error_post proc_desc location (fun summary -> (error, summary)) astate


(* the access error and summary must come from [summary_error_of_error] *)
let report_summary_error ({InterproceduralAnalysis.tenv; proc_desc} as analysis_data) path
    ((access_error : AccessResult.error), summary) : _ ExecutionDomain.base_t option =
  match access_error with
  | PotentialInvalidAccess {address; must_be_valid} ->
      let invalidation = Invalidation.ConstantDereference IntLit.zero in
      let access_trace = fst must_be_valid in
      let is_constant_deref_without_invalidation =
        is_constant_deref_without_invalidation invalidation access_trace
      in
      let is_suppressed =
        is_suppressed tenv proc_desc ~is_nullptr_dereference:true
          ~is_constant_deref_without_invalidation ~is_optional_empty:false
      in
      if is_suppressed then L.d_printfln "suppressed error" ;
      if Config.pulse_report_latent_issues then
        report analysis_data ~latent:true ~is_suppressed
          (AccessToInvalidAddress
             { calling_context= []
             ; invalid_address= address
             ; invalidation= ConstantDereference IntLit.zero
             ; invalidation_trace=
                 Immediate {location= Procdesc.get_loc proc_desc; history= ValueHistory.epoch}
             ; access_trace
             ; may_depend_on_an_unknown_value=
                 AbductiveDomain.Summary.contains_unknown_values summary
             ; must_be_valid_reason= snd must_be_valid } ) ;
      Some
        (Stopped (LatentInvalidAccess {astate= summary; address; must_be_valid; calling_context= []})
        )
  | PotentialInvalidSpecializedCall {specialized_type; trace} ->
      Some (Stopped (LatentSpecializedTypeIssue {astate= summary; specialized_type; trace}))
  | ReportableError {diagnostic} -> (
      let is_nullptr_dereference =
        match diagnostic with AccessToInvalidAddress _ -> true | _ -> false
      in
      let is_constant_deref_without_invalidation =
        is_constant_deref_without_invalidation_diagnostic diagnostic
      in
      let is_optional_empty = is_optional_empty diagnostic in
      let is_suppressed =
        is_suppressed tenv proc_desc ~is_nullptr_dereference ~is_constant_deref_without_invalidation
          ~is_optional_empty
      in
      match LatentIssue.should_report summary diagnostic with
      | `ReportNow -> (
          let trace_to_issue =
            Trace.Immediate {location= Procdesc.get_loc proc_desc; history= ValueHistory.epoch}
          in
          match current_specialization () with
          | Some specialization
            when (not (has_exposed_context ()))
                 && depends_on_requested_callable specialization diagnostic
                 && Diagnostic.aborts_execution path diagnostic ->
              let report =
                { ExecutionDomain.origin=
                    { proc_name= Procdesc.get_proc_name proc_desc
                    ; specialization= Some (Pulse specialization) }
                ; original_diagnostic= diagnostic
                ; diagnostic
                ; is_suppressed
                ; trace_to_issue }
              in
              register_conditional report ;
              Some (Stopped (ConditionalManifestIssue {astate= summary; report}))
          | _ ->
              if is_suppressed then L.d_printfln "ReportNow suppressed error" ;
              report analysis_data ~latent:false ~is_suppressed diagnostic ;
              if Diagnostic.aborts_execution path diagnostic then
                Some (Stopped (AbortProgram {astate= summary; diagnostic; trace_to_issue}))
              else None )
      | `DelayReport latent_issue ->
          if is_suppressed then L.d_printfln "DelayReport suppressed error" ;
          if Config.pulse_report_latent_issues then
            report_latent_issue analysis_data ~is_suppressed latent_issue ;
          Some (Stopped (LatentAbortProgram {astate= summary; latent_issue})) )
  | WithSummary _ ->
      (* impossible thanks to prior application of [summary_error_of_error] *)
      assert false


let report_error ({InterproceduralAnalysis.proc_desc} as analysis_data) path location access_error =
  let open SatUnsat.Import in
  summary_error_of_error proc_desc location access_error >>| report_summary_error analysis_data path


let report_errors analysis_data path location errors =
  let open SatUnsat.Import in
  List.rev errors
  |> List.fold ~init:(Sat None) ~f:(fun sat_result error ->
      match sat_result with
      | Unsat _ | Sat (Some _) ->
          sat_result
      | Sat None ->
          report_error analysis_data path location error )


let report_exec_results analysis_data path location results =
  let results = PulseTaintOperations.dedup_reports results in
  List.filter_map results ~f:(fun exec_result ->
      match PulseResult.to_result exec_result with
      | Ok post ->
          Some post
      | Error errors -> (
        match report_errors analysis_data path location errors with
        | Unsat unsat_info ->
            L.d_printfln "UNSAT discovered during error reporting" ;
            SatUnsat.log_unsat unsat_info ;
            None
        | Sat None -> (
          match exec_result with
          | Ok _ | FatalError _ ->
              L.die InternalError
                "report_errors returned None but the result was not a recoverable error"
          | Recoverable (exec_state, _) ->
              Some exec_state )
        | Sat (Some exec_state) ->
            Some exec_state ) )


let report_results analysis_data path location results =
  let open PulseResult.Let_syntax in
  List.map results ~f:(fun result ->
      let+ astate = result in
      ExecutionDomain.ContinueProgram astate )
  |> report_exec_results analysis_data path location


let report_result analysis_data path location result =
  report_results analysis_data path location [result]
