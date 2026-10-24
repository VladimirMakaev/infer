(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

open! IStd
module AbductiveDomain = PulseAbductiveDomain
module AbstractValue = PulseAbstractValue
module CallEvent = PulseCallEvent
module DecompilerExpr = PulseDecompilerExpr
module ValueHistory = PulseValueHistory
module ValueOrigin = PulseValueOrigin

(** {1 Wrapper to access the [PulseDecompiler.t] inside [AbductiveDomain.t]; all of the interface of
    [PulseDecompiler] is duplicated for ease of use} *)

val add_call_source :
     AbstractValue.t
  -> CallEvent.t
  -> ((AbstractValue.t * ValueHistory.t) * Typ.t) list
  -> AbductiveDomain.t
  -> AbductiveDomain.t

val find : AbstractValue.t -> AbductiveDomain.t -> DecompilerExpr.t

val find_from_origin : ValueOrigin.t -> AbductiveDomain.t -> DecompilerExpr.t
(** Like {!find}, but names the memory cell the value was loaded from when there is one. The
    decompiler keeps one expression per value, so a value shared with unrelated expressions, every
    null pointer and every zero of a path among them, carries whichever name was recorded last; the
    origin of a load is not shared. Falls back to {!find}. *)

val add_iterator_source :
     AbstractValue.t
  -> CallEvent.t
  -> src:AbstractValue.t
  -> is_reference_receiver:bool
  -> AbductiveDomain.t
  -> AbductiveDomain.t
