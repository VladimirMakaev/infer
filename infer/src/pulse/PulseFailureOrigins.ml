(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

open! IStd
open PulseBasicInterface
open PulseDomainInterface

(* Which calls' failure values does this state pin to null?

   This has to be asked BEFORE the state is turned into a summary. Summarising runs
   [discard_unreachable_], and the value a [CHECK] rejected is typically dead by then -- a
   procedure that returns [rhi->x] keeps the int, not the pointer -- so the attribute row is
   dropped and the question can no longer be answered. Every caller of this module is therefore
   at a point where it still holds the unsummarised state. *)

(* Asked by identity with the formula's own representative for 0, for the reason spelled out in
   [PulseSummary.forces_null]: the direct queries read only some of the stores and answer false
   on a state that prints [{v = 0}]. This is the same question on an unsummarised state. *)
let forces_null phi v =
  let phi, zero = Formula.absval_of_int phi IntLit.zero in
  AbstractValue.equal (Formula.get_var_repr phi v) (Formula.get_var_repr phi zero)


let fold_origins ~want (astate : AbductiveDomain.t) =
  let post = (astate.post :> BaseDomain.t) in
  BaseAddressAttributes.fold
    (fun value attrs acc ->
      let origins = Attribute.Attributes.get_failure_outcome attrs in
      if Attribute.FailureOriginSet.is_empty origins then acc
      else if
        Bool.equal want
          (forces_null astate.path_condition (AbductiveDomain.CanonValue.downcast value))
      then Attribute.FailureOriginSet.union origins acc
      else acc )
    post.BaseDomain.attrs Attribute.FailureOriginSet.empty


(* capped on the way out, because this one is stored in the exit and so travels with the
   disjunct; the cap is the attribute's own, so the two cannot disagree about how many origins a
   value is allowed to have *)
let pinned_null astate =
  Attribute.FailureOriginSet.union_capped (fold_origins ~want:true astate)
    Attribute.FailureOriginSet.empty


let not_pinned_null astate = fold_origins ~want:false astate
