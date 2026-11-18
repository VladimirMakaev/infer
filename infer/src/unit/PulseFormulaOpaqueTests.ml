(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

open! IStd
open OUnit2
module F = Format
module Term = Pulselib.PulseFormulaTerm
module Atom = Pulselib.PulseFormulaAtom
module LinArith = Pulselib.PulseFormulaLinArit
module Var = Pulselib.PulseFormulaVar
module SatUnsat = Pulselib.PulseSatUnsat
module Q = Pulselib.QSafeCapped
module Phi = Pulselib.PulseFormulaPhi

let get_sat = function
  | SatUnsat.Sat result ->
      result
  | SatUnsat.Unsat _ ->
      assert_failure "representable execution was discarded"


let normalize term = Atom.eval_term ~is_neq_zero:(fun _ -> false) term |> get_sat

let propagation_state () =
  let w = Var.mk_fresh () in
  let x = Var.mk_fresh () in
  let y = Var.mk_fresh () in
  let z = Var.mk_fresh () in
  let q = Q.of_float 0.1 in
  let q_squared = Q.mul q q in
  let phi, _ =
    Phi.add_linear_eq x
      (LinArith.add (LinArith.of_var y) (LinArith.mult q_squared (LinArith.of_var z)))
      Phi.ttrue
  in
  let phi, _ = Phi.add_linear_eq w (LinArith.mult q (LinArith.of_var y)) phi in
  (phi, w, x)


let assert_opaque_atom phi =
  assert_bool "propagation failure did not record an opaque constraint"
    (Atom.Set.exists (function Atom.Equal (Term.Opaque _, _) -> true | _ -> false) phi.Phi.atoms)


let assert_fixpoint term =
  let fixed = normalize term in
  assert_bool "expected an explicit opaque term"
    (match fixed with Term.Opaque _ -> true | _ -> false) ;
  for _ = 1 to 1000 do
    assert_bool "opaque normalization changed its fixed point" (Term.equal fixed (normalize fixed)) ;
    assert_bool "identity substitution changed opaque syntax"
      (Term.equal fixed (Term.subst_variables fixed ~f:(fun v -> LinArith.VarSubst v)))
  done ;
  fixed


(* Descends into a nested [Opaque], which [fold_map_direct_subterms] does not: the bound is on
   what the substitution walk visits, and that walk does recurse into a nested payload. *)
let rec term_size t =
  match t with
  | Term.Opaque sub ->
      1 + term_size sub
  | _ ->
      fst (Term.fold_map_direct_subterms t ~init:1 ~f:(fun acc sub -> (acc + term_size sub, sub)))


let rec term_depth t =
  match t with
  | Term.Opaque sub ->
      1 + term_depth sub
  | _ ->
      1
      + fst
          (Term.fold_map_direct_subterms t ~init:0 ~f:(fun acc sub ->
               (Int.max acc (term_depth sub), sub) ) )


let opaque_payload what = function
  | Term.Opaque payload ->
      payload
  | t ->
      assert_failure (F.asprintf "%s is not opaque: %a" what (Term.pp_no_paren Var.pp) t)


(* wide and shallow: 2^levels leaves, so it passes the depth cap and fails the size one *)
let rec balanced levels =
  if levels <= 0 then Term.Var (Var.mk_fresh ())
  else Term.Add (balanced (levels - 1), balanced (levels - 1))


(* deep and narrow: [links] + 1 deep and 2 * [links] + 1 nodes, the other way round *)
let rec chain links = if links <= 0 then Term.zero else Term.Add (Term.one, chain (links - 1))

let assert_collapsed what payload =
  (* the stand-in is a bare variable, so the payload is neither retained nor re-walked *)
  assert_bool
    (F.asprintf "%s kept a payload of size %d depth %d" what (term_size payload)
       (term_depth payload) )
    (match payload with Term.Var _ -> true | _ -> false)


let tests =
  "pulse_opaque_numeric_terms"
  >::: [ ( "constant multiplication"
         >:: fun _ ->
         assert_fixpoint (Term.Mult (Term.Const (Q.of_float 1e300), Term.Const (Q.of_int 2)))
         |> ignore )
       ; ( "coefficient composition"
         >:: fun _ ->
         let x = Var.mk_fresh () in
         let c = Term.Const (Q.of_float 0.1) in
         assert_fixpoint (Term.Mult (c, Term.Mult (c, Term.Mult (c, Term.Var x)))) |> ignore )
       ; ( "linear substitution"
         >:: fun _ ->
         let x = Var.mk_fresh () in
         let y = Var.mk_fresh () in
         let q = Q.of_float 0.1 in
         let linear = LinArith.mult q (LinArith.mult q (LinArith.of_var x)) in
         let replacement = LinArith.mult q (LinArith.of_var y) in
         let term =
           Term.subst_variables (Term.Linear linear) ~f:(fun v ->
               if Var.equal v x then LinArith.LinSubst replacement else LinArith.VarSubst v )
         in
         let fixed = assert_fixpoint term in
         let variables = Term.fold_variables fixed ~init:[] ~f:(fun vars v -> v :: vars) in
         assert_bool "old variable survived substitution"
           (not (List.mem variables x ~equal:Var.equal)) ;
         assert_bool "substituted variable was hidden" (List.mem variables y ~equal:Var.equal) )
       ; ( "uint64 bitwise folding"
         >:: fun _ ->
         let maximum = Z.(shift_left one 64 - one) |> Q.of_bigint in
         assert_fixpoint (Term.BitAnd (Term.Const maximum, Term.one)) |> ignore )
       ; ( "capped substitution calls each stateful callback once"
         >:: fun _ ->
         let x = Var.mk_fresh () in
         let y = Var.mk_fresh () in
         let z = Var.mk_fresh () in
         let q = Q.of_float 0.1 in
         let linear =
           LinArith.add (LinArith.of_var x) (LinArith.mult (Q.mul q q) (LinArith.of_var y))
         in
         let calls = ref 0 in
         let count, term =
           Term.fold_subst_variables (Term.Linear linear) ~init:0 ~f_subst:(fun count v ->
               incr calls ;
               ( count + 1
               , if Var.equal v y then LinArith.LinSubst (LinArith.mult q (LinArith.of_var z))
                 else LinArith.VarSubst v ) )
         in
         assert_equal 2 !calls ;
         assert_equal 2 count ;
         assert_fixpoint term |> ignore )
       ; ( "non-rational input does not prove non-integer"
         >:: fun _ ->
         Term.eval_const_shallow (Term.IsInt (Term.Const Q.undef, Typ.IInt))
         |> get_sat |> assert_fixpoint |> ignore )
       ; ( "uint64 shift folding"
         >:: fun _ ->
         let high = Z.shift_left Z.one 63 |> Q.of_bigint in
         assert_fixpoint (Term.BitShiftRight (Term.Const high, Term.one)) |> ignore )
       ; ( "unrepresentable symbolic shift count"
         >:: fun _ ->
         let x = Var.mk_fresh () in
         let count = Z.shift_left Z.one 64 |> Q.of_bigint in
         Term.simplify_shallow (Term.BitShiftLeft (Term.Var x, Term.Const count))
         |> get_sat |> assert_fixpoint |> ignore )
       ; ( "type forces zero through capped propagation"
         >:: fun _ ->
         let phi, _, x = propagation_state () in
         let phi, _ = Phi.add_dynamic_type x (Typ.mk (Tint IInt)) phi in
         let phi, _ =
           Phi.Normalizer.and_dynamic_type x (Typ.mk (Tfloat FDouble)) (phi, RevList.empty)
           |> get_sat
         in
         assert_opaque_atom phi )
       ; ( "opaque term merge does not retry capped propagation"
         >:: fun _ ->
         let phi, w, x = propagation_state () in
         let term = Term.Mult (Term.Const (Q.of_float 1e300), Term.Const (Q.of_int 2)) in
         let phi, _ = Phi.add_term_eq (normalize term) w phi in
         let phi, _ = Phi.Normalizer.and_var_term x term (phi, RevList.empty) |> get_sat in
         assert_opaque_atom phi )
       ; ( "division by zero remains undefined"
         >:: fun _ ->
         assert_bool "undefined division was accepted"
           ( match Term.eval_const_shallow (Term.DivI (Term.one, Term.zero)) with
           | SatUnsat.Unsat _ ->
               true
           | SatUnsat.Sat _ ->
               false ) )
       ; ( "nonconstant modulo by zero remains undefined"
         >:: fun _ ->
         let x = Var.mk_fresh () in
         assert_bool "undefined modulo was accepted"
           ( match Term.simplify_shallow (Term.Mod (Term.Add (Term.Var x, Term.one), Term.zero)) with
           | SatUnsat.Unsat _ ->
               true
           | SatUnsat.Sat _ ->
               false ) )
       ; ( "a payload within both caps is kept as it is"
         >:: fun _ ->
         let payload = Term.Add (Term.Var (Var.mk_fresh ()), Term.one) in
         assert_bool "a small payload was dropped"
           (phys_equal payload (opaque_payload "small" (Term.mk_opaque payload))) )
       ; ( "an over-size payload is dropped even when it is shallow"
         >:: fun _ ->
         (* 2^9 - 1 = 511 nodes at depth 10: over the size cap of 256, under the depth cap of
            32, so this fails on size alone and a size-blind bound would keep it *)
         let payload = balanced 8 in
         assert_equal ~printer:Int.to_string 511 (term_size payload) ;
         assert_bool "the fixture is not shallower than the depth cap" (term_depth payload <= 32) ;
         assert_collapsed "an over-size payload" (opaque_payload "wide" (Term.mk_opaque payload)) )
       ; ( "an over-depth payload is dropped even when it is small"
         >:: fun _ ->
         (* 81 nodes at depth 41: under the size cap, over the depth cap, the mirror of the
            case above, so neither cap can be the only one doing the work *)
         let payload = chain 40 in
         assert_equal ~printer:Int.to_string 81 (term_size payload) ;
         assert_bool "the fixture is not smaller than the size cap" (term_size payload <= 256) ;
         assert_collapsed "an over-depth payload" (opaque_payload "deep" (Term.mk_opaque payload))
         )
       ; ( "a nested opaque counts towards the caps"
         >:: fun _ ->
         (* [fold_map_direct_subterms] reports [Opaque] as having no sub-terms, so counting a
            nested payload as one node would let k levels of nesting hold max_size ^ k. The
            substitution walk does recurse into a nested payload, so the bound must too. *)
         let under_cap () = opaque_payload "under cap" (Term.mk_opaque (balanced 7)) in
         let nested = Term.Add (Term.Opaque (under_cap ()), Term.Opaque (under_cap ())) in
         assert_collapsed "a pair of in-cap payloads"
           (opaque_payload "nested" (Term.mk_opaque nested)) )
       ; ( "each collapse mints its own value"
         >:: fun _ ->
         (* one shared stand-in would make two unrelated uncomputable values equal *)
         let one () = opaque_payload "collapsed" (Term.mk_opaque (balanced 8)) in
         assert_bool "two collapsed payloads are the same value"
           (not (Term.equal (one ()) (one ()))) )
       ; ( "substitution that grows a payload past the cap collapses it"
         >:: fun _ ->
         (* the path that matters: a payload under the cap when it was built, enlarged by
            substituting a variable for a bigger term. A bound applied only at first
            construction would be bypassed by exactly the mechanism that causes the blow-up. *)
         let x = Var.mk_fresh () in
         let small = Term.mk_opaque (Term.Var x) in
         let big =
           List.init 300 ~f:(fun _ -> LinArith.of_var (Var.mk_fresh ()))
           |> List.fold ~init:(LinArith.of_var (Var.mk_fresh ())) ~f:LinArith.add
         in
         let grown =
           Term.subst_variables small ~f:(fun v ->
               if Var.equal v x then LinArith.LinSubst big else LinArith.VarSubst v )
         in
         assert_collapsed "a payload grown by substitution" (opaque_payload "grown" grown) ;
         (* and it is a fixpoint afterwards: the stand-in carries no equalities, so the next
            pass finds nothing to substitute and does not mint a second value *)
         assert_bool "the collapsed term was rebuilt again"
           (phys_equal grown (Term.subst_variables grown ~f:(fun v -> LinArith.VarSubst v))) ) ]
