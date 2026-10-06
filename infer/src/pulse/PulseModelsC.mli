(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

open! IStd
open PulseBasicInterface
open PulseModelsImport

val alloc_common :
  null_case:bool -> initialize:bool -> desc:string -> Attribute.allocator -> Exp.t option -> model
(** allocation that may also return null when [null_case] is true *)

val apply_unknown_callee_effect_on_actuals :
     desc:string
  -> ValueHistory.t
  -> (AbstractValue.t * Typ.t) list
  -> PulseAbductiveDomain.t
  -> PulseAbductiveDomain.t
(** what an unknown callee may do to the actuals, given with the types of the formals: write through
    the pointers to non-const and take ownership of the file descriptors passed by value *)

val matchers : matcher list
