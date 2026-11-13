(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

(* The unary operators PulseCItv matches on. *)

(* [ppx_compare] and [ppx_enumerate] reach for these by name. Inside Infer they arrive with
   [open! IStd]; here they are given directly rather than pulling in a standard-library
   replacement for the sake of two functions. *)
let compare_unit = Unit.compare

let equal_unit = Unit.equal

let all_unit = [()]

let compare_option = Option.compare

let equal_option = Option.equal

let all_option xs = None :: List.map (fun x -> Some x) xs

type t = Neg | BNot | LNot [@@deriving compare, equal, enumerate]

(* See the comment in Binop.ml: [all_of_t] is generated from the type and is what the oracle
   iterates; the walk below is the independent second enumeration it is checked against. *)
let succ (u : t) : t option = match u with Neg -> Some BNot | BNot -> Some LNot | LNot -> None

let walk : t list =
  let rec go u = u :: (match succ u with None -> [] | Some n -> go n) in
  go Neg
