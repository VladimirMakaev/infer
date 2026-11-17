(* Differential/property pilot for the VERBATIM infer/src/pulse/PulseFormulaLinArit.ml.

   The oracle is exact rational evaluation using plain zarith [Q] -- deliberately NOT
   QSafeCapped, whose [mul] is the capped operation under test. If the oracle used the same
   capped arithmetic it could not see a capping defect at all.

     eval s l  =  get_constant_part l  +  SUM over (v,q) in fold l of  q * s(v)

   Every exported operation should commute with eval. Properties are taken verbatim from the
   docstrings in PulseFormulaLinArit.mli. *)

module L = PulseFormulaLinArit
module Var = PulseFormulaVar

let seed = try int_of_string Sys.argv.(1) with _ -> 20261116

let iters = try int_of_string Sys.argv.(2) with _ -> 20000

let big = try Sys.argv.(3) = "big" with _ -> false

(* ---------- oracle: exact rational evaluation, plain Q ---------- *)

let eval (s : (int * Q.t) list) (l : L.t) : Q.t =
  L.fold l ~init:(L.get_constant_part l) ~f:(fun acc (v, q) ->
      let sv = try List.assoc v s with Not_found -> Q.zero in
      Q.add acc (Q.mul q sv) )


let coeffs l = L.fold l ~init:[] ~f:(fun acc (v, q) -> (v, q) :: acc)

let is_rational q = match Q.classify q with Q.ZERO | Q.NZERO -> true | _ -> false

(* does this linear form carry a poisoned (non-rational) coefficient or constant? *)
let poisoned l =
  (not (is_rational (L.get_constant_part l)))
  || List.exists (fun (_, q) -> not (is_rational q)) (coeffs l)


let pp_l l =
  let b = Buffer.create 64 in
  let fmt = Format.formatter_of_buffer b in
  L.pp (fun fmt v -> Format.fprintf fmt "%a" Var.pp v) fmt l ;
  Format.pp_print_flush fmt () ;
  Buffer.contents b


let pp_q q = if is_rational q then Q.to_string q else "UNDEF/" ^ Q.to_string q

(* ---------- generators ---------- *)

let st = Random.State.make [|seed|]

let gen_small_q () =
  let n = Random.State.int st 21 - 10 in
  let d = 1 + Random.State.int st 6 in
  Q.make (Z.of_int n) (Z.of_int d)


(* occasionally generate a coefficient large enough to trip QSafeCapped's 128-bit cap *)
let gen_q () =
  if big && Random.State.int st 100 < 25 then
    let k = 1 + Random.State.int st 3 in
    let n = Z.pow (Z.of_int (1 + Random.State.int st 1000000)) (3 * k) in
    let d = Z.pow (Z.of_int (1 + Random.State.int st 97)) k in
    Q.make n d
  else gen_small_q ()


let gen_var () = Random.State.int st 6

(* NOTE: QSafeCapped.mul short-circuits when either operand is one, and [of_var] has
   coefficient 1, so a SINGLE [mult] can never cap. The first version of this generator
   therefore never reached the capped path and reported a clean run for the wrong reason.
   Scale a second time so composed multiplication can actually overflow. *)
let gen_lin () =
  let n = Random.State.int st 4 in
  let rec go acc i =
    if i >= n then acc else go (L.add acc (L.mult (gen_q ()) (L.of_var (gen_var ())))) (i + 1)
  in
  let l = go (L.of_q (gen_q ())) 0 in
  if big && Random.State.int st 100 < 50 then L.mult (gen_q ()) l else l


let gen_subst () = List.init 6 (fun v -> (v, gen_small_q ()))

(* ---------- reporting ---------- *)

let reached = ref 0

let failures = Hashtbl.create 16

let shown = Hashtbl.create 16

(* POST-FIX CONTRACT (85711c62). LinArith no longer stores a capped coefficient: [mult] and
   [of_q] call [check_rational] and raise [NotRepresentable], which the Phi layer catches and
   turns into an opaque marker atom. This harness has no Phi layer, so the exception is the
   observable marker here, and catching it is how we model "Pulse abstained".

   Two counters, because one without the other would be meaningless:
     abstained -- how often the cap FIRED. This is the reach count. A run where the cap never
                  fires proves nothing, exactly as in the pre-fix pilot.
     poisoned  -- how many surviving forms still carry a non-rational coefficient. Under the
                  fix this MUST be 0: that is the whole claim. Pre-fix it was 2755/20000. *)
let abstained = ref 0

let report name detail =
  let n = try Hashtbl.find failures name with Not_found -> 0 in
  Hashtbl.replace failures name (n + 1) ;
  if not (Hashtbl.mem shown name) then begin
    Hashtbl.replace shown name true ;
    Printf.printf "\n*** %s ***\n%s\n" name detail
  end


let qeq a b =
  (* both sides undefined counts as agreement; the point is to catch one side being poisoned *)
  if (not (is_rational a)) && not (is_rational b) then true else Q.equal a b


(* ---------- properties ---------- *)

let () =
  Printf.printf "seed=%d iters=%d big_coefficients=%b\n" seed iters big ;
  for _ = 1 to iters do
    try
      let l1 = gen_lin () and l2 = gen_lin () in
      if poisoned l1 then incr reached ;
      let s = gen_subst () in
      let e1 = eval s l1 and e2 = eval s l2 in
      (* P11/P12: TAUTOLOGIES. These hold for EVERY linear form, poisoned or not, so they are
       checked outside the `poisoned` guard -- that guard is what hid the defect in the first
       pass. l - l is zero, and solving l = l is always true. *)
      if not (L.is_zero (L.subtract l1 l1)) then
        report "P12 subtract l l is not zero"
          (Printf.sprintf "  l = %s\n  l - l = %s" (pp_l l1) (pp_l (L.subtract l1 l1))) ;
      ( match L.solve_eq l1 l1 with
      | Sat None ->
          ()
      | Sat (Some (x, sol)) ->
          report "P11 solve_eq l l returned a SOLUTION instead of `always true`"
            (Printf.sprintf "  l = %s\n  solve_eq l l = Sat (x%d = %s)" (pp_l l1) x (pp_l sol))
      | Unsat _ ->
          report "P11b solve_eq l l returned UNSAT" (pp_l l1) ) ;
      (* P0: no operation should silently manufacture a non-rational coefficient *)
      List.iter
        (fun (nm, l) ->
          if poisoned l && not (poisoned l1 || poisoned l2) then
            report
              ("P0 " ^ nm ^ " manufactured a non-rational coefficient from rational inputs")
              (Printf.sprintf "  l1 = %s\n  l2 = %s\n  %s = %s" (pp_l l1) (pp_l l2) nm (pp_l l)) )
        [("add l1 l2", L.add l1 l2); ("subtract l1 l2", L.subtract l1 l2); ("minus l1", L.minus l1)] ;
      if not (poisoned l1 || poisoned l2) then begin
        (* P1-P3: additive structure commutes with eval *)
        if not (qeq (eval s (L.add l1 l2)) (Q.add e1 e2)) then
          report "P1 add does not commute with eval"
            (Printf.sprintf "  l1 = %s\n  l2 = %s\n  eval(add) = %s\n  sum       = %s" (pp_l l1)
               (pp_l l2)
               (pp_q (eval s (L.add l1 l2)))
               (pp_q (Q.add e1 e2)) ) ;
        if not (qeq (eval s (L.minus l1)) (Q.neg e1)) then
          report "P2 minus does not commute with eval" (pp_l l1) ;
        if not (qeq (eval s (L.subtract l1 l2)) (Q.sub e1 e2)) then
          report "P3 subtract does not commute with eval"
            (Printf.sprintf "  l1 = %s\n  l2 = %s" (pp_l l1) (pp_l l2)) ;
        (* P4: scaling *)
        let q = gen_small_q () in
        let scaled = L.mult q l1 in
        if (not (poisoned scaled)) && not (qeq (eval s scaled) (Q.mul q e1)) then
          report "P4 mult does not commute with eval"
            (Printf.sprintf "  q = %s\n  l = %s\n  eval(mult) = %s\n  q*eval     = %s"
               (Q.to_string q) (pp_l l1)
               (pp_q (eval s scaled))
               (pp_q (Q.mul q e1)) ) ;
        (* P6: get_as_const agrees with eval, and a constant must have no variables *)
        ( match L.get_as_const l1 with
        | Some c ->
            if not (qeq c e1) then
              report "P6 get_as_const disagrees with eval"
                (Printf.sprintf "  l = %s\n  get_as_const = %s\n  eval         = %s" (pp_l l1)
                   (pp_q c) (pp_q e1) ) ;
            if coeffs l1 <> [] then
              report "P6b get_as_const returned Some on a form with variables" (pp_l l1)
        | None ->
            () ) ;
        (* P5: solve_eq, straight from the docstring *)
        ( match L.solve_eq l1 l2 with
        | Sat (Some (x, l)) ->
            let s' = (x, eval s l) :: List.remove_assoc x s in
            let a = eval s' l1 and b = eval s' l2 in
            if (not (qeq a b)) && not (poisoned l) then
              report "P5 solve_eq: substituting the solution does not equate the two sides"
                (Printf.sprintf "  l1 = %s\n  l2 = %s\n  solved: x%d = %s\n  lhs = %s  rhs = %s"
                   (pp_l l1) (pp_l l2) x (pp_l l) (pp_q a) (pp_q b) )
        | Sat None ->
            if not (qeq e1 e2) then
              report "P5b solve_eq said ALWAYS TRUE but the sides differ under an assignment"
                (Printf.sprintf "  l1 = %s\n  l2 = %s\n  eval l1 = %s\n  eval l2 = %s" (pp_l l1)
                   (pp_l l2) (pp_q e1) (pp_q e2) )
        | Unsat _ ->
            if qeq e1 e2 then
              report "P5c solve_eq said UNSAT but an assignment satisfies the equality"
                (Printf.sprintf "  l1 = %s\n  l2 = %s\n  both evaluate to %s" (pp_l l1) (pp_l l2)
                   (pp_q e1) ) ) ;
        (* P7: pivot, straight from the docstring:
           [pivot (v,q) l] assumes v occurs in l with coefficient q
           and returns l' such that l' = -(1/q)*(l - q*v) *)
        ( match coeffs l1 with
        | (v, q) :: _ when is_rational q && not (Q.equal q Q.zero) ->
            let piv = L.pivot (v, q) l1 in
            if not (poisoned piv) then begin
              (* expected, computed with exact Q: -(1/q) * (l - q*v) *)
              let l_minus_qv = Q.sub e1 (Q.mul q (try List.assoc v s with Not_found -> Q.zero)) in
              let expect = Q.neg (Q.div l_minus_qv q) in
              let got = eval s piv in
              if not (qeq got expect) then
                report "P7 pivot disagrees with its own docstring formula"
                  (Printf.sprintf
                     "  l = %s\n\
                     \  pivot on (x%d, %s)\n\
                     \  pivot(l) = %s\n\
                     \  eval(pivot) = %s\n\
                     \  -(1/q)*(l - q*v) = %s"
                     (pp_l l1) v (Q.to_string q) (pp_l piv) (pp_q got) (pp_q expect) )
            end
        | _ ->
            () ) ;
        (* P10: classify_minimized_maximized's stated meaning *)
        match L.classify_minimized_maximized l1 with
        | `Minimized ->
            if List.exists (fun (_, q) -> is_rational q && Q.sign q < 0) (coeffs l1) then
              report "P10 `Minimized but a coefficient is negative" (pp_l l1)
        | `Maximized ->
            if List.exists (fun (_, q) -> is_rational q && Q.sign q > 0) (coeffs l1) then
              report "P10b `Maximized but a coefficient is positive" (pp_l l1)
        | `Constant ->
            if coeffs l1 <> [] then report "P10c `Constant but there are variables" (pp_l l1)
        | `Neither ->
            ()
      end
    with L.NotRepresentable -> incr abstained
  done ;
  Printf.printf "\n==== summary (seed %d, %d iterations, big=%b) ====\n" seed iters big ;
  Printf.printf "  REACH  cap fired, abstained via NotRepresentable: %d / %d iterations\n"
    !abstained iters ;
  Printf.printf "  CLAIM  surviving forms carrying a capped (non-rational) coefficient: %d\n"
    !reached ;
  if Hashtbl.length failures = 0 then print_endline "  no property violations"
  else Hashtbl.iter (fun k n -> Printf.printf "  %6d  %s\n" n k) failures
