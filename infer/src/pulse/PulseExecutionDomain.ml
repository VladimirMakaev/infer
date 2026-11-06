(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

open! IStd
module F = Format
open PulseBasicInterface
module AbductiveDomain = PulseAbductiveDomain
module DecompilerExpr = PulseDecompilerExpr
module Diagnostic = PulseDiagnostic
module LatentIssue = PulseLatentIssue

type conditional_manifest_report =
  { origin: (SpecializedProcname.t[@yojson.opaque])
  ; original_diagnostic: Diagnostic.t
  ; is_suppressed: bool
  ; diagnostic: Diagnostic.t
  ; trace_to_issue: Trace.t }
[@@deriving equal, compare, yojson_of]

(* Why a path stopped, and where. By the time a path becomes [ExitProgram] the call that ended it
   is gone, so this is the only place the information survives.

   [Fatal] is a crash the program chose: an assert, a CHECK, a fatal log. [NormalExit] is an
   orderly end -- [exit], [_exit], a thread exit -- which is not a defect. [UnknownExit] is any
   other [noreturn] callee, whose intent its name does not tell us; a rule that reports on fatal
   paths must stay silent on those rather than guess. *)
type exit_kind = Fatal | NormalExit | UnknownExit [@@deriving equal, compare, yojson_of]

type exit_info =
  { location: Location.t  (** the call that ended the path, not the procedure's own exit node *)
  ; kind: exit_kind
  ; fatal_callee: Procname.t option
        (** the callee that ended it, when one is known: a trace into it is what makes a report
            about an abort readable. [None] for an exit the frontend produced without a call. *) }
[@@deriving equal, compare, yojson_of]

let pp_exit_kind fmt = function
  | Fatal ->
      F.pp_print_string fmt "Fatal"
  | NormalExit ->
      F.pp_print_string fmt "NormalExit"
  | UnknownExit ->
      F.pp_print_string fmt "UnknownExit"


(* Ends the thread or the process in an orderly way: not a defect, and not something a
   fatal-path rule may report. These are the standard C and POSIX spellings. *)
let is_orderly_exit procname =
  List.mem
    ["exit"; "_exit"; "_Exit"; "quick_exit"; "pthread_exit"; "thrd_exit"]
    (Procname.get_method procname) ~equal:String.equal


(* Classify a [[noreturn]] callee that has no model.

   The rule is generic, and deliberately carries no list of names to recognise as fatal. A
   function declared [[noreturn]] that is not one of the orderly exits above does not return and
   does not exit: it crashes, and that is true of a platform assert ([__assert_fail],
   [__assert2], [__android_log_assert]), of a fatal logger, and of a project's own
   [[noreturn]] check helper alike, without Infer having to know any of their names. Functions
   that abort WITHOUT being [[noreturn]] never reach here -- the user names them in
   [--pulse-model-abort], and that model declares [Fatal] where it is registered.

   Two shapes are classified fatal here that are not crashes: a [[noreturn]] function that
   throws, and one that loops forever. Both are rare, and both cost precision in a later rule
   rather than correctness here. *)
let exit_kind_of_no_return_callee procname = if is_orderly_exit procname then NormalExit else Fatal

(* The type variable is needed to distinguish summaries from plain states.

   Some of the variants have summary-typed states instead of plain states, to ensure we have
   normalized them and don't need to normalize them again. *)
type stopped_execution =
  | ExitProgram of {astate: AbductiveDomain.Summary.t; exit: exit_info}
  | AbortProgram of
      {astate: AbductiveDomain.Summary.t; diagnostic: Diagnostic.t; trace_to_issue: Trace.t}
  | LatentAbortProgram of {astate: AbductiveDomain.Summary.t; latent_issue: LatentIssue.t}
  | LatentInvalidAccess of
      { astate: AbductiveDomain.Summary.t
      ; address: DecompilerExpr.t
      ; must_be_valid: Trace.t * Invalidation.must_be_valid_reason option
      ; calling_context: (CallEvent.t * Location.t) list }
  | LatentSpecializedTypeIssue of
      {astate: AbductiveDomain.Summary.t; specialized_type: Typ.Name.t; trace: Trace.t}
  | ConditionalManifestIssue of
      {astate: AbductiveDomain.Summary.t; report: conditional_manifest_report}
[@@deriving equal, compare, yojson_of, variants]

type 'abductive_domain_t base_t =
  | ContinueProgram of 'abductive_domain_t
  | ExceptionRaised of 'abductive_domain_t
  | Stopped of stopped_execution
[@@deriving equal, compare, yojson_of, variants]

type t = AbductiveDomain.t base_t

let continue astate = ContinueProgram astate

let summary_of_stopped_execution = function
  | ExitProgram {astate}
  | AbortProgram {astate}
  | LatentAbortProgram {astate}
  | LatentInvalidAccess {astate}
  | ConditionalManifestIssue {astate}
  | LatentSpecializedTypeIssue {astate} ->
      astate


let leq_stopped_execution ~lhs ~rhs =
  phys_equal lhs rhs
  ||
  match (lhs, rhs) with
  | AbortProgram {astate= astate1}, AbortProgram {astate= astate2}
  | ExitProgram {astate= astate1}, ExitProgram {astate= astate2} ->
      (* deliberately NOT comparing [exit]: subsumption decides which disjuncts merge, so making
         it finer would grow summaries and spend disjunct budget, which changes reports on
         programs that have nothing to do with exits. Two stopped paths with equal states are the
         same path for every consumer that exists today. *)
      AbductiveDomain.Summary.leq ~lhs:astate1 ~rhs:astate2
  | ( LatentAbortProgram {astate= astate1; latent_issue= issue1}
    , LatentAbortProgram {astate= astate2; latent_issue= issue2} ) ->
      LatentIssue.equal issue1 issue2 && AbductiveDomain.Summary.leq ~lhs:astate1 ~rhs:astate2
  | ( LatentInvalidAccess {astate= astate1; address= v1; must_be_valid= _, reason_opt1}
    , LatentInvalidAccess {astate= astate2; address= v2; must_be_valid= _, reason_opt2} ) ->
      DecompilerExpr.equal v1 v2
      && Option.equal Invalidation.equal_must_be_valid_reason reason_opt1 reason_opt2
      && AbductiveDomain.Summary.leq ~lhs:astate1 ~rhs:astate2
  | ( ConditionalManifestIssue {astate= astate1; report= report1}
    , ConditionalManifestIssue {astate= astate2; report= report2} ) ->
      equal_conditional_manifest_report report1 report2
      && AbductiveDomain.Summary.leq ~lhs:astate1 ~rhs:astate2
  | _ ->
      false


let leq ~lhs ~rhs =
  phys_equal lhs rhs
  ||
  match (lhs, rhs) with
  | Stopped astate1, Stopped astate2 ->
      leq_stopped_execution ~lhs:astate1 ~rhs:astate2
  | ExceptionRaised astate1, ExceptionRaised astate2
  | ContinueProgram astate1, ContinueProgram astate2 ->
      AbductiveDomain.leq ~lhs:astate1 ~rhs:astate2
  | _ ->
      false


let to_astate = function
  | Stopped exec ->
      (summary_of_stopped_execution exec :> AbductiveDomain.t)
  | ExceptionRaised astate | ContinueProgram astate ->
      astate


let pp_header kind fmt = function
  | ContinueProgram _ ->
      F.pp_print_string fmt "ContinueProgram"
  | ExceptionRaised _ ->
      Pp.with_color kind Orange F.pp_print_string fmt "ExceptionRaised"
  | Stopped (AbortProgram {diagnostic}) ->
      Pp.with_color kind Red F.pp_print_string fmt "AbortProgram" ;
      let issue_type = Diagnostic.get_issue_type ~latent:false diagnostic in
      let message, _suggestion = Diagnostic.get_message_and_suggestion diagnostic in
      let location = Diagnostic.get_location diagnostic in
      F.fprintf fmt "(%a: %a: %s)" Location.pp location IssueType.pp issue_type message
  | Stopped (ExitProgram {exit= {location; kind= exit_kind; fatal_callee}}) ->
      Pp.with_color kind Orange F.pp_print_string fmt "ExitProgram" ;
      F.fprintf fmt "(%a, %a%a)" Location.pp location pp_exit_kind exit_kind
        (fun fmt -> function None -> () | Some pname -> F.fprintf fmt ", %a" Procname.pp pname)
        fatal_callee
  | Stopped (LatentAbortProgram {latent_issue}) ->
      Pp.with_color kind Orange F.pp_print_string fmt "LatentAbortProgram" ;
      let diagnostic = LatentIssue.to_diagnostic latent_issue in
      let issue_type = Diagnostic.get_issue_type ~latent:true diagnostic in
      let message, _suggestion = Diagnostic.get_message_and_suggestion diagnostic in
      let location = Diagnostic.get_location diagnostic in
      F.fprintf fmt "(%a: %a: %s)@ %a" Location.pp location IssueType.pp issue_type message
        LatentIssue.pp latent_issue
  | Stopped (LatentInvalidAccess {address; must_be_valid= _}) ->
      Pp.with_color kind Orange F.pp_print_string fmt "LatentInvalidAccess" ;
      F.fprintf fmt "(%a)" DecompilerExpr.pp address
  | Stopped (LatentSpecializedTypeIssue {specialized_type; trace}) ->
      Pp.with_color kind Orange F.pp_print_string fmt "LatentSpecializedTypeIssue" ;
      let origin_location = Trace.get_start_location trace in
      F.fprintf fmt "(%a: %a)" Location.pp origin_location Typ.Name.pp specialized_type
  | Stopped (ConditionalManifestIssue {report= {diagnostic}}) ->
      F.fprintf fmt "ConditionalManifestIssue(%a)" Diagnostic.pp diagnostic


let pp_with_kind kind path_opt fmt exec_state =
  F.fprintf fmt "%a%a" (pp_header kind) exec_state (PulsePp.pp kind path_opt) (to_astate exec_state)


let pp fmt (exec_state : 'abductive_domain_t base_t) =
  F.fprintf fmt "%a%a" (pp_header TEXT) exec_state AbductiveDomain.pp (to_astate exec_state)


type summary = AbductiveDomain.Summary.t base_t [@@deriving compare, equal, yojson_of]

let pp_summary pe fmt (exec_summary : summary) = pp_with_kind pe None fmt (exec_summary :> t)

let equal_fast_stopped_execution exec_state1 exec_state2 =
  phys_equal exec_state1 exec_state2
  ||
  match (exec_state1, exec_state2) with
  | ( AbortProgram {astate= astate1; diagnostic= diagnostic1}
    , AbortProgram {astate= astate2; diagnostic= diagnostic2} ) ->
      (* ignoring traces in [AbortProgram] *)
      phys_equal astate1 astate2 && phys_equal diagnostic1 diagnostic2
  | ExitProgram {astate= astate1}, ExitProgram {astate= astate2} ->
      (* same reasoning as [leq_stopped_execution] *)
      phys_equal astate1 astate2
  | ( ConditionalManifestIssue {astate= astate1; report= report1}
    , ConditionalManifestIssue {astate= astate2; report= report2} ) ->
      phys_equal astate1 astate2 && phys_equal report1 report2
  | _ ->
      false


let equal_fast exec_state1 exec_state2 =
  phys_equal exec_state1 exec_state2
  ||
  match (exec_state1, exec_state2) with
  | Stopped astate1, Stopped astate2 ->
      equal_fast_stopped_execution astate1 astate2
  | ContinueProgram astate1, ContinueProgram astate2
  | ExceptionRaised astate1, ExceptionRaised astate2 ->
      phys_equal astate1 astate2
  | _ ->
      false


let is_normal (exec_state : t) : bool =
  match exec_state with ExceptionRaised _ -> false | _ -> true


let is_exceptional (exec_state : t) : bool =
  match exec_state with ExceptionRaised _ -> true | _ -> false


let is_executable (exec_state : t) : bool =
  match exec_state with ContinueProgram _ | ExceptionRaised _ -> true | _ -> false


let exceptional_to_normal : t -> t = function
  | ExceptionRaised astate ->
      ContinueProgram astate
  | x ->
      x


let is_active_loop = function
  | ContinueProgram {AbductiveDomain.loop_invariant_under_inference= Some {header}} ->
      Some header
  | _ ->
      None


let to_name = Variants_of_base_t.to_name
