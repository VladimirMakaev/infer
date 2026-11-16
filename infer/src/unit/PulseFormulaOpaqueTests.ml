(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

open! IStd
open OUnit2
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
               false ) ) ]
