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
  Tenv.t -> (AbstractValue.t * ValueHistory.t) FuncArg.t -> desc:string -> model_no_non_disj option
(** [nullable_return tenv dest ~desc]: the null and non-null results that a callee returning a
    [std::unique_ptr] or a [std::shared_ptr] by value writes into [dest], if [dest] has one of these
    types *)

module SharedPtr : sig
  val assign_count :
       PathContext.t
    -> Location.t
    -> AbstractValue.t * ValueHistory.t
    -> constant:IntLit.t
    -> desc:string
    -> AbductiveDomain.t
    -> AbductiveDomain.t PulseOperationResult.t
end
