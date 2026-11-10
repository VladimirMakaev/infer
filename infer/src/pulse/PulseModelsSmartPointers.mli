(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

open! IStd
open PulseBasicInterface
open PulseDomainInterface
open PulseModelsImport

val matchers : matcher list

val unique_ptr_constructor :
     Tenv.t
  -> ValueOrigin.t FuncArg.t
  -> ValueOrigin.t FuncArg.t list
  -> desc:string
  -> model_no_non_disj option
(** Standard default and adopting-pointer constructors, including when their declarations were not
    captured. Arguments have already undergone reference forwarding. *)

val nullable_return :
     ?nullable_origin:Attribute.t
  -> Tenv.t
  -> (AbstractValue.t * ValueHistory.t) FuncArg.t
  -> desc:string
  -> model_no_non_disj option
(** [nullable_return tenv dest ~desc]: the null and non-null results that a callee returning a
    [std::unique_ptr] or a [std::shared_ptr] by value writes into [dest], if [dest] has one of these
    types. [nullable_origin] is attached to the managed pointer on the null result, so that a
    smart-pointer return can be a source for the same rules a raw pointer already is. *)

module SharedPtr : sig
  val assign_count :
       PathContext.t
    -> Location.t
    -> AbstractValue.t * ValueHistory.t
    -> constant:IntLit.t
    -> desc:string
    -> AbductiveDomain.t
    -> AbductiveDomain.t PulseOperationResult.t

  val copy_or_move_constructor :
       move:bool
    -> (AbstractValue.t * ValueHistory.t) FuncArg.t
    -> AbstractValue.t * ValueHistory.t
    -> desc:string
    -> model_no_non_disj
  (** [copy_or_move_constructor ~move dst src] increments the reference count when [move] is false
      and transfers it, emptying [src], when it is true. Exposed so that container models can apply
      it when the element's own constructor is not in the tenv: these models are keyed on the
      procname while a container's element lookup is keyed on the tenv, and the two disagree
      whenever the standard library is not captured. The caller chooses [move], because a
      container's insertion reaches its element through a forwarding reference and so cannot ask the
      callee's own value category: that keeps [const T&&], which no move constructor can bind. *)
end
