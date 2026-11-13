(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

(* The integer literals PulseCItv computes with. The real IntLit is a record of a signedness
   tag, an arbitrary-precision integer and a pointerness tag, and every operation PulseCItv
   uses -- addition, subtraction, negation, division, the comparisons, [min], [max] and the
   constants -- acts on the integer alone. A plain arbitrary-precision integer is therefore a
   faithful stand-in for the signed, non-pointer fragment, which is the fragment the oracle
   covers. The two tags are deliberately absent: the real [compare] orders a negative signed
   value and the same bit pattern read as unsigned differently, so an oracle that modelled
   them would have to model that order as well. Extending to the tagged domain is separate
   work and is recorded as a limitation in README.md. *)

type t = Z.t

let compare = Z.compare

let equal = Z.equal

let eq = Z.equal

let leq a b = Z.leq a b

let lt a b = Z.lt a b

let geq a b = Z.geq a b

let gt a b = Z.gt a b

let add = Z.add

let sub = Z.sub

let neg = Z.neg

(* [Z.div] truncates toward zero, as the real [IntLit.div] does: it is [Z.( / )] there. *)
let div = Z.div

let min = Z.min

let max = Z.max

let zero = Z.zero

let one = Z.one

let minus_one = Z.minus_one

let iszero z = Z.equal z Z.zero

let of_int = Z.of_int

let to_int = Z.to_int

let pp fmt z = Format.pp_print_string fmt (Z.to_string z)

let yojson_of_t z : Yojson.Safe.t = `String (Z.to_string z)
