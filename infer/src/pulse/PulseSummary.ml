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
open PulseOperationResult.Import

type pre_post_list = ExecutionDomain.summary list [@@deriving yojson_of]

(* "calling this exits fatally iff the value [deref_depth] dereferences into formal
   [formal_index] is null". [IsNull] is the only condition E9 needs; it is a variant so a later
   contract extends a match rather than silently redefining the field. *)
type exit_condition_kind = IsNull [@@deriving equal, compare, yojson_of]

type exit_condition = {formal_index: int; deref_depth: int; condition: exit_condition_kind}
[@@deriving equal, compare, yojson_of]

type summary =
  { pre_post_list: pre_post_list
  ; non_disj: NonDisjDomain.Summary.t
  ; exit_conditions: exit_condition list }
[@@deriving yojson_of]

type t =
  { main: summary
  ; specialized: summary Specialization.Pulse.Map.t
  ; failed: string Specialization.Pulse.Map.t }

let yojson_of_t {main; specialized} =
  `Assoc
    [ ("main", [%yojson_of: summary] main)
    ; ( "specialized"
      , [%yojson_of: (Specialization.Pulse.t * summary) list]
          (Specialization.Pulse.Map.bindings specialized) ) ]


let pp_pre_post_list print_kind fmt ~pp_specialized_name pre_posts =
  F.open_vbox 0 ;
  F.fprintf fmt "%d%t pre/post(s)@;" (List.length pre_posts) pp_specialized_name ;
  List.iteri pre_posts ~f:(fun i (pre_post : ExecutionDomain.summary) ->
      F.fprintf fmt "#%d: @[%a@]@;" i (ExecutionDomain.pp_summary print_kind) pre_post ) ;
  F.close_box ()


let pp_exit_condition fmt {formal_index; deref_depth; condition= IsNull} =
  F.fprintf fmt "formal#%d%s is null" formal_index (String.make deref_depth '*')


let pp_summary print_kind fmt ~pp_specialized_name {pre_post_list; non_disj; exit_conditions} =
  if not (List.is_empty exit_conditions) then
    F.fprintf fmt "ExitConditions: [%a]@\n" (Pp.seq ~sep:", " pp_exit_condition) exit_conditions ;
  F.fprintf fmt "%a%a"
    (pp_pre_post_list print_kind ~pp_specialized_name)
    pre_post_list
    (Pp.html_collapsible_block ~name:"Non-disjunctive state" print_kind NonDisjDomain.Summary.pp)
    non_disj


let pp_proc_name_sexp print_kind fmt specialized_proc_name =
  Pp.html_collapsible_block ~name:"s-exp for --procs-to-analyze" print_kind Sexp.pp_hum fmt
    (SpecializedProcname.sexp_of_t specialized_proc_name)


let pp_main_and_specialized {Pp.kind= print_kind} proc_name fmt {main; specialized} =
  if Specialization.Pulse.Map.is_empty specialized then
    pp_summary print_kind ~pp_specialized_name:(fun _fmt -> ()) fmt main
  else
    let pp_specialized_name fmt = F.pp_print_string fmt " main" in
    F.open_hvbox 0 ;
    F.fprintf fmt "%a@\n%a"
      (pp_summary print_kind ~pp_specialized_name)
      main (pp_proc_name_sexp print_kind)
      {SpecializedProcname.proc_name; specialization= None} ;
    Specialization.Pulse.Map.iter
      (fun specialization pre_posts ->
        let pp_specialized_name fmt =
          F.fprintf fmt " specialized with %a" Specialization.Pulse.pp specialization
        in
        F.fprintf fmt "@\n%a@\n%a"
          (pp_summary print_kind ~pp_specialized_name)
          pre_posts (pp_proc_name_sexp print_kind)
          {SpecializedProcname.proc_name; specialization= Some (Pulse specialization)} )
      specialized ;
    F.close_box ()


let pp pp_env proc_name fmt ({failed} as summary) =
  pp_main_and_specialized pp_env proc_name fmt summary ;
  Specialization.Pulse.Map.iter
    (fun specialization run ->
      F.fprintf fmt "@\nspecialization %a timed out in the run of %s" Specialization.Pulse.pp
        specialization run )
    failed


let add_disjunctive_pre_post pre_post {pre_post_list; non_disj; exit_conditions} =
  (* the added post is not one [exit_conditions] was computed from, so the contract no longer
     describes this summary; dropping it is the only answer that cannot be wrong *)
  ignore exit_conditions ;
  {pre_post_list= pre_post :: pre_post_list; non_disj; exit_conditions= []}


let empty = {pre_post_list= []; non_disj= NonDisjDomain.Summary.bottom; exit_conditions= []}

let join summary1 summary2 =
  let pre_post_list = summary1.pre_post_list @ summary2.pre_post_list in
  let non_disj = NonDisjDomain.Summary.join summary1.non_disj summary2.non_disj in
  (* A condition has to hold of EVERY fatal disjunct, so where both sides contribute fatal
     disjuncts only a condition both promise survives. A side with NO fatal disjunct constrains
     nothing and must not erase the other's conditions -- joining the exit-node summaries with
     the exception-sink ones goes through here, and the sink side is usually [empty], so a plain
     intersection silently discarded every condition. *)
  let has_fatal {pre_post_list} =
    List.exists pre_post_list ~f:(function
      | ExecutionDomain.Stopped (ExitProgram {exit= {kind= Fatal}}) ->
          true
      | _ ->
          false )
  in
  let exit_conditions =
    match (has_fatal summary1, has_fatal summary2) with
    | true, true ->
        List.filter summary1.exit_conditions ~f:(fun c ->
            List.mem summary2.exit_conditions c ~equal:equal_exit_condition )
    | true, false ->
        summary1.exit_conditions
    | false, true ->
        summary2.exit_conditions
    | false, false ->
        []
  in
  {pre_post_list; non_disj; exit_conditions}


let exec_summary_of_post_common ({InterproceduralAnalysis.proc_desc} as analysis_data) ~ignore_leaks
    ~continue_program ~exception_raised specialization path location
    (exec_astate : ExecutionDomain.t) : _ ExecutionDomain.base_t SatUnsat.t =
  let summarize (astate : AbductiveDomain.t)
      ~(exec_domain_of_summary : AbductiveDomain.Summary.summary -> 'a ExecutionDomain.base_t)
      ~(is_exceptional_state : bool) : _ ExecutionDomain.base_t SatUnsat.t =
    let open SatUnsat.Import in
    let+ summary_result =
      AbductiveDomain.Summary.of_post (Procdesc.get_attributes proc_desc) location astate
    in
    let summary_result =
      if ignore_leaks then AccessResult.ignore_leaks summary_result else summary_result
    in
    match (summary_result : _ result) with
    | Ok summary ->
        PulseReport.note_summary_escapes analysis_data summary ;
        exec_domain_of_summary summary
    | Error (`MemoryLeak (summary, astate, allocator, allocation_trace, location)) ->
        PulseReport.report_summary_error analysis_data path
          ( ReportableError
              { astate
              ; diagnostic= ResourceLeak {resource= Memory allocator; allocation_trace; location} }
          , summary )
        |> Option.value ~default:(exec_domain_of_summary summary)
    | Error (`JavaResourceLeak (summary, astate, class_name, allocation_trace, location)) ->
        PulseReport.report_summary_error analysis_data path
          ( ReportableError
              { astate
              ; diagnostic= ResourceLeak {resource= JavaClass class_name; allocation_trace; location}
              }
          , summary )
        |> Option.value ~default:(exec_domain_of_summary summary)
    | Error (`UnawaitedAwaitable (summary, astate, allocation_trace, location)) ->
        (* suppress unawaited awaitable reporting in the case that we're throwing an exception because it leads to
           too many true-but-unhelpful positives. TODO: reinstate reporting in the case that the exception is caught *)
        if is_exceptional_state then (
          L.d_printfln "Suppressing Unawaited Awaitable report because exception thown" ;
          exec_domain_of_summary summary )
        else
          PulseReport.report_summary_error analysis_data path
            ( ReportableError
                {astate; diagnostic= ResourceLeak {resource= Awaitable; allocation_trace; location}}
            , summary )
          |> Option.value ~default:(exec_domain_of_summary summary)
    | Error (`HackUnfinishedBuilder (summary, astate, allocation_trace, location, builder_type)) ->
        if is_exceptional_state then (
          L.d_printfln "Suppressing Unfinished Builder report because exception thown" ;
          exec_domain_of_summary summary )
        else
          PulseReport.report_summary_error analysis_data path
            ( ReportableError
                { astate
                ; diagnostic=
                    ResourceLeak
                      {resource= HackBuilderResource builder_type; allocation_trace; location} }
            , summary )
          |> Option.value ~default:(exec_domain_of_summary summary)
    | Error (`CSharpResourceLeak (summary, astate, class_name, allocation_trace, location)) ->
        PulseReport.report_summary_error analysis_data path
          ( ReportableError
              { astate
              ; diagnostic=
                  ResourceLeak {resource= CSharpClass class_name; allocation_trace; location} }
          , summary )
        |> Option.value ~default:(exec_domain_of_summary summary)
    | Error (`PotentialInvalidAccessSummary (summary, astate, address, must_be_valid)) -> (
      match
        let open IOption.Let_syntax in
        let* addr = DecompilerExpr.abstract_value_of_expr address in
        let* _, attrs = AbductiveDomain.find_post_cell_opt addr (astate :> AbductiveDomain.t) in
        Attributes.get_invalid attrs
      with
      | None ->
          ExecutionDomain.(
            Stopped
              (LatentInvalidAccess {astate= summary; address; must_be_valid; calling_context= []}) )
      | Some (invalidation, invalidation_trace) ->
          (* NOTE: this probably leads to the error being dropped as the access trace is unlikely to
             contain the reason for invalidation and thus we will filter out the report. TODO:
             figure out if that's a problem. *)
          PulseReport.report_summary_error analysis_data path
            ( ReportableError
                { diagnostic=
                    AccessToInvalidAddress
                      { calling_context= []
                      ; invalid_address= address
                      ; invalidation
                      ; invalidation_trace
                      ; access_trace= fst must_be_valid
                      ; may_depend_on_an_unknown_value= astate.AbductiveDomain.unknown_values
                      ; must_be_valid_reason= snd must_be_valid }
                ; astate }
            , summary )
          |> Option.value ~default:(exec_domain_of_summary summary) )
  in
  match exec_astate with
  | ExceptionRaised astate ->
      summarize astate ~exec_domain_of_summary:exception_raised ~is_exceptional_state:true
  | ContinueProgram astate ->
      summarize astate ~exec_domain_of_summary:continue_program ~is_exceptional_state:false
  (* already a summary but need to reconstruct the variants to make the type system happy :( *)
  | Stopped (AbortProgram {astate; diagnostic; trace_to_issue}) ->
      PulseReport.report_if_entry_point analysis_data trace_to_issue diagnostic ;
      Sat (Stopped (AbortProgram {astate; diagnostic; trace_to_issue}))
  | Stopped (ConditionalManifestIssue {astate; report}) ->
      PulseReport.register_conditional report ;
      PulseReport.note_summary_escapes analysis_data astate ;
      if Option.is_none specialization || PulseReport.has_exposed_context () then (
        PulseReport.report_conditional_origin analysis_data report ;
        Sat
          (Stopped
             (AbortProgram
                {astate; diagnostic= report.diagnostic; trace_to_issue= report.trace_to_issue} ) ) )
      else Sat (Stopped (ConditionalManifestIssue {astate; report}))
  | Stopped (ExitProgram {astate; exit}) ->
      Sat (Stopped (ExitProgram {astate; exit}))
  | Stopped (LatentAbortProgram {astate; latent_issue}) ->
      Sat (Stopped (LatentAbortProgram {astate; latent_issue}))
  | Stopped (LatentInvalidAccess {astate; address; must_be_valid; calling_context}) ->
      Sat (Stopped (LatentInvalidAccess {astate; address; must_be_valid; calling_context}))
  | Stopped (LatentSpecializedTypeIssue {astate; specialized_type; trace}) -> (
    match specialization with
    | Some specialization
      when Specialization.Pulse.has_type_in_specialization specialization specialized_type ->
        Sat (Stopped (LatentSpecializedTypeIssue {astate; specialized_type; trace}))
    | _ ->
        let diagnostic =
          Diagnostic.HackCannotInstantiateAbstractClass {type_name= specialized_type; trace}
        in
        PulseReport.report analysis_data ~is_suppressed:false ~latent:false diagnostic ;
        Sat
          (Stopped
             (AbortProgram
                { astate
                ; diagnostic
                ; trace_to_issue=
                    Immediate {location= Procdesc.get_loc proc_desc; history= ValueHistory.epoch} }
             ) ) )


let force_exit_program ~ignore_leaks ~exit analysis_data path post =
  exec_summary_of_post_common analysis_data None path post ~ignore_leaks
    ~continue_program:(fun astate -> Stopped (ExitProgram {astate; exit}))
    ~exception_raised:(fun astate -> Stopped (ExitProgram {astate; exit}))


(* A helper's contract for a caller: "you exit fatally here if this argument is null".

   The condition is keyed on a formal INDEX plus a DEREFERENCE DEPTH, not on a formal alone. A
   plain pointer parameter is constrained at depth 1, but the shipped precondition macros take a
   reference parameter and constrain its POINTEE, which is depth 2. Keyed on the index alone the
   reference case silently never matches, which is the FP-free but useless answer.

   [IsNull] is the only condition E9 needs. It is a variant rather than a bare flag so that a
   later slice adding, say, a negative-return contract extends a match instead of changing the
   meaning of the existing field. *)
let max_deref_depth = 2

(* the value reached by dereferencing [pvar] [depth] times in a disjunct's PRE, if the pre
   reaches that far: the pre only contains what the callee actually touched.

   Edge targets come back needing canonicalisation, so each step goes through the summary's own
   equality relation; comparing a non-canonical value against the formula would silently answer
   "unconstrained" for a value the formula does in fact pin down. *)
let value_at_formal_in_pre (astate : AbductiveDomain.Summary.t) pvar depth =
  let as_astate = (astate :> AbductiveDomain.t) in
  let canon v = AbductiveDomain.CanonValue.canon as_astate v in
  let pre = AbductiveDomain.Summary.get_pre astate in
  let rec walk v n =
    if n <= 0 then Some (AbductiveDomain.CanonValue.downcast v)
    else
      match BaseMemory.find_edge_opt v Dereference pre.BaseDomain.heap with
      | Some (v', _) ->
          walk (canon v') (n - 1)
      | None ->
          None
  in
  BaseStack.find_opt (Var.of_pvar pvar) pre.BaseDomain.stack
  |> Option.bind ~f:(fun vo -> walk (canon (ValueOrigin.value vo)) depth)


(* [phi] forces [v] to be null. Asked by identity with the formula's own representative for the
   constant 0: [absval_of_int] is documented to return the same abstract variable each time the
   same integer is queried, so "v IS the zero variable" is exactly the fact [term_eqs: 0=v] holds.

   Measured alternatives that all answer FALSE on a state that prints {v = 0}: is_known_zero
   (reads only intervals and linear equalities), PulseArithmetic.is_known_zero, and pruning the
   negation and looking for Unsat (the prunes do not re-solve against term_eqs or atoms). Pulse's
   formula is deliberately incomplete for speed, so "the state prints it" does not imply any
   given query sees it.

   Weakening this to "not known to be non-null" would be wrong in the other direction: a path
   that never constrained the value at all would pass, and every helper would look
   discriminating. *)
let implies_null astate v =
  let phi = AbductiveDomain.Summary.get_path_condition astate in
  let phi, zero = Formula.absval_of_int phi IntLit.zero in
  AbstractValue.equal (Formula.get_var_repr phi v) (Formula.get_var_repr phi zero)


let exit_conditions_of_posts proc_desc pre_post_list =
  let fatal_pres =
    List.filter_map pre_post_list ~f:(function
      | ExecutionDomain.Stopped (ExitProgram {astate; exit= {kind= Fatal}}) ->
          Some astate
      | _ ->
          None )
  in
  (* nothing to promise if the callee never exits fatally *)
  if List.is_empty fatal_pres then []
  else
    let continue_pres =
      List.filter_map pre_post_list ~f:(function
        | ExecutionDomain.ContinueProgram astate ->
            Some astate
        | _ ->
            None )
    in
    let holds_in astate pvar depth ~f =
      match value_at_formal_in_pre astate pvar depth with None -> false | Some v -> f astate v
    in
    List.concat_mapi (Procdesc.get_pvar_formals proc_desc) ~f:(fun formal_index (pvar, _typ) ->
        List.filter_map
          (List.range 1 (max_deref_depth + 1))
          ~f:(fun deref_depth ->
            (* EVERY fatal exit must be conditioned on this value being null, and at least one
               surviving path must prove it is not: the first makes the condition necessary, the
               second makes it discriminating rather than "this helper always aborts" *)
            if
              List.for_all fatal_pres ~f:(fun astate ->
                  holds_in astate pvar deref_depth ~f:implies_null )
              && List.exists continue_pres ~f:(fun astate ->
                  holds_in astate pvar deref_depth ~f:(fun astate v -> not (implies_null astate v)) )
            then Some {formal_index; deref_depth; condition= IsNull}
            else None ) )


let of_posts ({InterproceduralAnalysis.proc_desc} as analysis_data) specialization location posts
    non_disj =
  let pre_post_list =
    List.filter_mapi posts ~f:(fun i (exec_state, path) ->
        L.d_printfln "Creating spec out of state #%d:@\n%a" i
          (ExecutionDomain.pp_with_kind HTML None)
          exec_state ;
        exec_summary_of_post_common analysis_data specialization path location exec_state
          ~ignore_leaks:false
          ~continue_program:(fun astate -> ContinueProgram astate)
          ~exception_raised:(fun astate -> ExceptionRaised astate)
        |> SatUnsat.sat )
  in
  { pre_post_list
  ; non_disj= NonDisjDomain.make_summary (Procdesc.get_attributes proc_desc) location non_disj
  ; exit_conditions= exit_conditions_of_posts proc_desc pre_post_list }


let mk_objc_self_pvar proc_name = Pvar.mk Mangled.self proc_name

let init_fields_zero tenv path location ~zero addr typ astate =
  let get_fields typ =
    match typ.Typ.desc with
    | Tstruct struct_name ->
        Tenv.lookup tenv struct_name |> Option.map ~f:(fun {Struct.fields} -> fields)
    | _ ->
        None
  in
  let rec init_fields_zero_helper addr typ astate =
    match get_fields typ with
    | Some fields ->
        List.fold fields ~init:(Ok astate) ~f:(fun acc {Struct.name= field; typ= field_typ} ->
            let* acc in
            let acc, field_addr = Memory.eval_edge addr (FieldAccess field) acc in
            init_fields_zero_helper field_addr field_typ acc )
    | None ->
        PulseOperations.write_deref path location ~ref:addr ~obj:zero astate
  in
  init_fields_zero_helper addr typ astate


(** Constructs summary [{self = 0} {return = self}] when [proc_desc] returns a POD type. This allows
    us to connect invalidation with invalid access in the trace *)
let mk_nil_messaging_summary_aux tenv proc_name (proc_attrs : ProcAttributes.t) =
  let path = PathContext.initial in
  let t0 = path.PathContext.timestamp in
  let self = mk_objc_self_pvar proc_name in
  let astate = AbductiveDomain.mk_initial tenv proc_attrs in
  let** astate, (self_value, self_history) =
    PulseOperations.eval_deref path proc_attrs.loc (Lvar self) astate
  in
  let** astate = PulseArithmetic.prune_eq_zero self_value astate in
  let event = ValueHistory.NilMessaging (proc_attrs.loc, t0) in
  let updated_self_value_hist = (self_value, ValueHistory.sequence event self_history) in
  match List.last proc_attrs.formals with
  | Some (last_formal, {desc= Tptr (typ, _)}, _) when Mangled.is_return_param last_formal ->
      let ret_param_var = Pvar.get_ret_param_pvar proc_name in
      let** astate, ret_param_var_addr_hist =
        PulseOperations.eval_deref path proc_attrs.loc (Lvar ret_param_var) astate
      in
      Sat
        (init_fields_zero tenv path proc_attrs.loc ~zero:updated_self_value_hist
           ret_param_var_addr_hist typ astate )
  | _ ->
      let ret_var = Pvar.get_ret_pvar proc_name in
      let** astate, ret_var_addr_hist =
        PulseOperations.eval path Write proc_attrs.loc (Lvar ret_var) astate
      in
      Sat
        (PulseOperations.write_deref path proc_attrs.loc ~ref:ret_var_addr_hist
           ~obj:updated_self_value_hist astate )


let mk_latent_non_POD_nil_messaging tenv proc_name (proc_attrs : ProcAttributes.t) =
  let path = PathContext.initial in
  let self = mk_objc_self_pvar proc_name in
  let astate = AbductiveDomain.mk_initial tenv proc_attrs in
  let** astate, (self_value, _self_history) =
    PulseOperations.eval_deref path proc_attrs.loc (Lvar self) astate
  in
  let trace = Trace.Immediate {location= proc_attrs.loc; history= ValueHistory.epoch} in
  let** astate = PulseArithmetic.prune_eq_zero self_value astate in
  let++ summary =
    let open SatUnsat.Import in
    AbductiveDomain.Summary.of_post proc_attrs proc_attrs.loc astate
    >>| AccessResult.ignore_leaks >>| AccessResult.of_abductive_summary_result
    >>| AccessResult.with_summary
  in
  ExecutionDomain.(
    Stopped
      (LatentInvalidAccess
         { astate= summary
         ; address= Decompiler.find self_value astate
         ; must_be_valid=
             (trace, Some (SelfOfNonPODReturnMethod (ProcAttributes.to_return_type proc_attrs)))
         ; calling_context= [] } ) )


let mk_objc_nil_messaging_summary tenv (proc_attrs : ProcAttributes.t) =
  let proc_name = proc_attrs.proc_name in
  if Procname.is_objc_instance_method proc_name then (
    if proc_attrs.is_ret_type_pod then (
      (* In ObjC, when a method is called on nil, there is no NPE, the method is actually not called
         and the return value is 0/false/nil.  We create a nil summary to avoid reporting NPE in
         this case.  However, there is an exception in the case where the return type is non-POD.
         In that case it's UB and we want to report an error. *)
      match
        (let** astate = mk_nil_messaging_summary_aux tenv proc_name proc_attrs in
         let open SatUnsat.Import in
         AbductiveDomain.Summary.of_post proc_attrs proc_attrs.loc astate
         >>| AccessResult.ignore_leaks >>| AccessResult.of_abductive_summary_result
         >>| AccessResult.with_summary )
        |> PulseOperationResult.sat_ok
      with
      | Some summary ->
          Some (ContinueProgram summary)
      | None ->
          L.internal_error
            "mk_nil_messaging_summary_aux for %a resulted in an error or an unsat state" Procname.pp
            proc_name ;
          None )
    else
      match
        mk_latent_non_POD_nil_messaging tenv proc_name proc_attrs |> PulseOperationResult.sat_ok
      with
      | Some _ as some_summary ->
          some_summary
      | None ->
          L.internal_error
            "mk_latent_non_POD_nil_messaging for %a resulted in an error or an unsat state"
            Procname.pp proc_name ;
          None )
  else None


let positive_allocated_self proc_name location self_address astate =
  (* For objc the method is called only if self>0, so we use [prune_positive] to send [self>0] to
     the precondition instead of relying on the fact that [self] is allocated (which is done in
     [AbductiveDomain.mk_initial]).

     It's important to do the prune *first* before the dereference to detect contradictions if the
     address is equal to 0 *)
  let astate =
    if Procname.is_objc_method proc_name then
      PulseArithmetic.prune_positive (fst self_address) astate
    else Sat (Ok astate)
  in
  astate
  >>|= PulseOperations.eval_access PathContext.initial Read location self_address Dereference
  >>|| fst


let initial_with_positive_self (proc_attrs : ProcAttributes.t) initial_astate =
  match ProcAttributes.get_this proc_attrs with
  | Some self_var -> (
      let result =
        let** astate, self_address =
          PulseOperations.eval_deref PathContext.initial proc_attrs.loc (Lvar self_var)
            initial_astate
        in
        positive_allocated_self proc_attrs.proc_name proc_attrs.loc self_address astate
      in
      match PulseOperationResult.sat_ok result with
      | Some astate ->
          astate
      | None ->
          L.internal_error
            "found an error or an unsat state when adding [self > 0] to %a's initial state %a"
            AbductiveDomain.pp initial_astate Procname.pp proc_attrs.proc_name ;
          initial_astate )
  | None ->
      initial_astate


let append_objc_actual_self_positive proc_name (proc_attrs : ProcAttributes.t) self_actual astate =
  match self_actual with
  | Some (self, _) when Procname.is_objc_instance_method proc_name ->
      positive_allocated_self proc_name proc_attrs.loc self astate
  | _ ->
      Sat (Ok astate)


let add_failed specialization summary =
  let run = ResultsDir.RunState.current_run () in
  {summary with failed= Specialization.Pulse.Map.add specialization run summary.failed}


let is_failed specialization {failed} =
  Specialization.Pulse.Map.find_opt specialization failed
  |> Option.exists ~f:(String.equal (ResultsDir.RunState.current_run ()))


let merge x y =
  let merged_is_same_to_x = ref true in
  let merged_is_same_to_y = ref true in
  let merged =
    Specialization.Pulse.Map.merge
      (fun _ x y ->
        match (x, y) with
        | None, None | Some _, Some _ ->
            x
        | Some _, None ->
            merged_is_same_to_y := false ;
            x
        | None, Some _ ->
            merged_is_same_to_x := false ;
            y )
      x.specialized y.specialized
  in
  let merged =
    if !merged_is_same_to_x then x
    else if !merged_is_same_to_y then y
    else {x with specialized= merged}
  in
  let current_run = ResultsDir.RunState.current_run () in
  let failed =
    Specialization.Pulse.Map.union (fun _ run _ -> Some run) x.failed y.failed
    |> Specialization.Pulse.Map.filter (fun specialization run ->
        String.equal run current_run
        && not (Specialization.Pulse.Map.mem specialization merged.specialized) )
  in
  if Specialization.Pulse.Map.equal String.equal failed merged.failed then merged
  else {merged with failed}
