(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

(* Stand-in for Infer's Binop.  The constructor set is copied from the real Binop so that
   the two matches in PulseCItv.ml (abduce_binop_constraints, binop) are exhaustive in
   exactly the same way; the real [ikind option] payload is modelled as [int option]. *)

open! IStd

type ikind_opt = int option [@@deriving compare, equal, yojson_of]

type t =
  | PlusA of ikind_opt
  | PlusPI
  | MinusA of ikind_opt
  | MinusPI
  | MinusPP
  | Mult of ikind_opt
  | DivI
  | DivF
  | Mod
  | Shiftlt
  | Shiftrt
  | Lt
  | Gt
  | Le
  | Ge
  | Eq
  | Ne
  | BAnd
  | BXor
  | BOr
  | LAnd
  | LOr
[@@deriving compare, equal, yojson_of]
