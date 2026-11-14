(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

(* Stand-in for Infer's IntLit, carrying the tags.
 *
 * An earlier version of this stub was [type t = int], which the translator maps to Rocq's
 * unbounded Z.  That was WRONG, and not only for overflow: the real IntLit orders literals
 * by (area, i), where area is 1 for negative Signed, 2 for non-negative, and 3 for negative
 * UNSIGNED.  So a negative unsigned literal is GREATER than every non-negative one, and the
 * plain Z order disagrees with the module under test.  Two executed witnesses:
 *     IntLit.leq unsigned(-1) signed(0)  =  false      (Z order says true)
 *     abduce_le (equal_to unsigned(-1)) (equal_to signed(0))  =  Unsatisfiable
 * Both are regression checks in v/Soundness.v now.
 *
 * This version mirrors infer/src/IR/IntLit.ml: the record, [area], [compare_value], and the
 * [lift1]/[lift2] tag joins.  What is still a stand-in: the [i] field is OCaml [int] rather
 * than [Z.t], which the translator renders as Rocq's unbounded [Z] - so the VALUE domain is
 * faithful and only the OCaml-side typechecking uses a machine int.  Pointerness is carried
 * exactly as the real one does, including that it never affects the ordering
 * (compare_pointerness is constant 0 there). *)

open! IStd

type signedness = Signed | Unsigned [@@deriving compare, equal]

type pointerness = NotPointer | Pointer [@@deriving equal]

(* as in the real module: pointerness is carried but never ordered on *)
let compare_pointerness _ _ = 0

type t = {signedness: signedness; i: int; pointerness: pointerness} [@@deriving compare, equal]

(* as in the real module, written by hand rather than derived, and over the value only *)
let yojson_of_t (x : t) = [%yojson_of: string] (string_of_int x.i)

let join_signedness s1 s2 = match (s1, s2) with Signed, Signed -> Signed | _ -> Unsigned

let join_pointerness p1 p2 =
  match (p1, p2) with NotPointer, NotPointer -> NotPointer | _ -> Pointer


let area (x : t) = match (x.i < 0, x.signedness) with
  | true, Signed ->
      (* negative signed *) 1
  | false, _ ->
      (* non-negative *) 2
  | true, Unsigned ->
      (* negative unsigned *) 3


(* lexicographic on (area, i), exactly [%compare: int * Z.t] in the real module *)
let compare_value (x : t) (y : t) =
  let ax = area x and ay = area y in
  if ax < ay then -1 else if ax > ay then 1 else if x.i < y.i then -1 else if x.i > y.i then 1 else 0


let eq x y = compare_value x y = 0

let neq x y = compare_value x y <> 0

let leq x y = compare_value x y <= 0

let lt x y = compare_value x y < 0

let geq x y = compare_value x y >= 0

let gt x y = compare_value x y > 0

let lift1 f (x : t) = {x with i= f x.i}

let lift2 f (x : t) (y : t) =
  { signedness= join_signedness x.signedness y.signedness
  ; i= f x.i y.i
  ; pointerness= join_pointerness x.pointerness y.pointerness }


let of_int i = {signedness= Signed; i; pointerness= NotPointer}

let zero = of_int 0

let one = of_int 1

let minus_one = of_int (-1)

let iszero (x : t) = x.i = 0

let neg x = lift1 (fun v -> -v) x

let add x y = lift2 (fun a b -> a + b) x y

let sub x y = lift2 (fun a b -> a - b) x y

let div x y = lift2 (fun a b -> a / b) x y

let min x y = if leq x y then x else y

let max x y = if geq x y then x else y

let to_int (x : t) = x.i

(* Mirrors IntLit.ml:172, which SHADOWS the equal that [@@deriving equal] generates on the
   record above.  So the exported equality is the area-based one, not structural: it ignores
   pointerness entirely (compare_pointerness is constant 0 there) and it identifies
   signed(0) with unsigned(0), since both are in area 2 with the same value.  Getting this
   wrong is what the second review round caught - see the witness_equal_* checks. *)
let equal i1 i2 = eq i1 i2

let pp fmt (x : t) = Format.pp_print_int fmt x.i
