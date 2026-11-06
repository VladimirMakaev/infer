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

type exit_kind = Fatal | NormalExit | UnknownExit [@@deriving equal, compare, yojson_of]

type exit_info =
  { location: Location.t
  ; kind: exit_kind
  ; fatal_callee: Procname.t option
  ; failure_origins: (Attribute.FailureOriginSet.t[@yojson.opaque])
        (** the calls whose documented failure value was null when this path ended. Snapshotted here
            because the state is summarised the moment it becomes an exit, and summarising discards
            the attribute: see [PulseFailureOrigins]. Filled in by whoever turns a state into an
            exit, so a construction site may leave it empty. *) }
[@@deriving equal, compare, yojson_of]

val exit_kind_of_no_return_callee : Procname.t -> exit_kind
(** classify a [noreturn] callee that has no model. Models declare their kind where they are
    registered; this is the name-based fallback, and it answers [UnknownExit] rather than guessing.
*)

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

type 'abductive_domain_t base_t =
  | ContinueProgram of 'abductive_domain_t
  | ExceptionRaised of 'abductive_domain_t
  | Stopped of stopped_execution

type t = AbductiveDomain.t base_t

include AbstractDomain.Disjunct with type t := t

val pp_with_kind : Pp.print_kind -> PulsePathContext.t option -> F.formatter -> t -> unit

val pp : F.formatter -> t -> unit

val continue : AbductiveDomain.t -> t

val summary_of_stopped_execution : stopped_execution -> AbductiveDomain.Summary.t

type summary = AbductiveDomain.Summary.t base_t [@@deriving compare, equal, yojson_of]

val pp_summary : Pp.print_kind -> F.formatter -> summary -> unit

val to_name : 'a base_t -> string
