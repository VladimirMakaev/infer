(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

(* The binary operators PulseCItv matches on, in the order they appear in infer/src/IR/Binop.ml.
   The integer-kind argument of the arithmetic operators is not inspected by this module, so it
   is given as unit here rather than pulling in the whole type environment. *)

(* [ppx_compare] and [ppx_enumerate] reach for these by name. Inside Infer they arrive with
   [open! IStd]; here they are given directly rather than pulling in a standard-library
   replacement for the sake of two functions. *)
let compare_unit = Unit.compare

let equal_unit = Unit.equal

let all_unit = [()]

let compare_option = Option.compare

let equal_option = Option.equal

let all_option xs = None :: List.map (fun x -> Some x) xs

type t =
  | PlusA of unit option
  | PlusPI
  | MinusA of unit option
  | MinusPI
  | MinusPP
  | Mult of unit option
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
[@@deriving compare, equal, enumerate]

(* [all_of_t] above is generated from the type declaration, so it is the one description of
   this domain that cannot drift: adding a constructor, or a payload shape to one, extends it
   without anyone remembering to. The oracle iterates it, and checks it against the independent
   walk below by set equality. Hand-written facts about how many shapes there are have been
   removed; they were correct and still said nothing, because nothing made them grow with the
   type. *)

(* A second, independent enumeration: a successor walk. Its match has no catch-all and spells
   out both payload shapes of the three operators that carry one, so dropping a shape is a
   compile error; and the oracle requires the walk and [all_of_t] to be the same SET, so a
   successor wired past a shape, a truncation at either end, a repeat, and a constructor added
   to the type but not to the walk are all refused at run time. *)
let succ (b : t) : t option =
  match b with
  | PlusA None ->
      Some (PlusA (Some ()))
  | PlusA (Some ()) ->
      Some PlusPI
  | PlusPI ->
      Some (MinusA None)
  | MinusA None ->
      Some (MinusA (Some ()))
  | MinusA (Some ()) ->
      Some MinusPI
  | MinusPI ->
      Some MinusPP
  | MinusPP ->
      Some (Mult None)
  | Mult None ->
      Some (Mult (Some ()))
  | Mult (Some ()) ->
      Some DivI
  | DivI ->
      Some DivF
  | DivF ->
      Some Mod
  | Mod ->
      Some Shiftlt
  | Shiftlt ->
      Some Shiftrt
  | Shiftrt ->
      Some Lt
  | Lt ->
      Some Gt
  | Gt ->
      Some Le
  | Le ->
      Some Ge
  | Ge ->
      Some Eq
  | Eq ->
      Some Ne
  | Ne ->
      Some BAnd
  | BAnd ->
      Some BXor
  | BXor ->
      Some BOr
  | BOr ->
      Some LAnd
  | LAnd ->
      Some LOr
  | LOr ->
      None


let walk : t list =
  let rec go b = b :: (match succ b with None -> [] | Some n -> go n) in
  go (PlusA None)
