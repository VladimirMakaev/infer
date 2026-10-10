(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

open! IStd
open PulseBasicInterface
open PulseDomainInterface

[@@@warning "-unused-value-declaration"]

val report :
  _ InterproceduralAnalysis.t -> is_suppressed:bool -> latent:bool -> Diagnostic.t -> unit

val report_if_entry_point : _ InterproceduralAnalysis.t -> Trace.t -> Diagnostic.t -> unit

val with_specialization :
  _ InterproceduralAnalysis.t -> Specialization.Pulse.t option -> f:(unit -> 'a) -> 'a

val register_conditional : ExecutionDomain.conditional_manifest_report -> unit

val has_exposed_context : unit -> bool

val mark_incomplete : unit -> unit

val report_conditional_origin :
  _ InterproceduralAnalysis.t -> ExecutionDomain.conditional_manifest_report -> unit

val note_unknown_actuals :
  Tenv.t -> ((AbstractValue.t * ValueHistory.t) * Typ.t) list -> AbductiveDomain.t -> unit

val has_trivial_callable_actual :
  Tenv.t -> ((AbstractValue.t * ValueHistory.t) * Typ.t) list -> AbductiveDomain.t -> bool

val note_summary_escapes : _ InterproceduralAnalysis.t -> AbductiveDomain.Summary.t -> unit

val report_summary_error :
     _ InterproceduralAnalysis.t
  -> PathContext.t
  -> AccessResult.error * AbductiveDomain.Summary.t
  -> _ ExecutionDomain.base_t option
(** [None] means that the execution can continue but we could not compute the continuation state
    (because this only takes a [AccessResult.error], which doesn't have the ok state) *)

val report_result :
     _ InterproceduralAnalysis.t
  -> PathContext.t
  -> Location.t
  -> AbductiveDomain.t AccessResult.t
  -> ExecutionDomain.t list

val report_results :
     _ InterproceduralAnalysis.t
  -> PathContext.t
  -> Location.t
  -> AbductiveDomain.t AccessResult.t list
  -> ExecutionDomain.t list

val report_exec_results :
     _ InterproceduralAnalysis.t
  -> PathContext.t
  -> Location.t
  -> ExecutionDomain.t AccessResult.t list
  -> ExecutionDomain.t list
