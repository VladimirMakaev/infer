(* Class B (normalisation idempotence) and class E (order independence) on the VERBATIM
   PulseFormulaTerm.ml and PulseFormulaAtom.ml, frontier 1a41747f.

   Class B is a SELF-DECLARED residual: 266403af62a says "more work is needed here to make
   sure that the formula is always in normal form".

   Properties, all guard-free (they hold for every input, normalised or not):
     B1  eval_const_shallow is a fixpoint:   eval(eval t) = eval t
     B2  simplify_shallow is a fixpoint
     B3  Atom.eval is a fixpoint: re-evaluating an already-evaluated atom returns it unchanged
     B4  Atom.nnot is an involution:          nnot (nnot a) = a
     E1  commutative operators fold the same both ways (Add, Mult, Equal, NotEqual, And, Or)
     E2  Atom.eval agrees on a comparison and its mirror (a<=b  vs  b>=a as LessEqual swapped)

   Every condition has a REACH COUNTER: a fixpoint property over inputs that are already
   fixpoints is vacuous, so "how often did the FIRST application change anything" is the
   number that decides whether a clean run means anything. *)

module T = PulseFormulaTerm
module A = PulseFormulaAtom

(* Atom.equal is SHADOWED at PulseFormulaAtom.ml:77 by a smart constructor
   (let equal t1 t2 = Equal (t1, t2)), so the derived structural equality is not reachable
   under that name. compare is not shadowed. *)
let aeq a b = Int.equal (A.compare a b) 0

let reach : (string, int) Hashtbl.t = Hashtbl.create 16

let hit k = Hashtbl.replace reach k (1 + try Hashtbl.find reach k with Not_found -> 0)

let failures : (string, int) Hashtbl.t = Hashtbl.create 16

let shown : (string, unit) Hashtbl.t = Hashtbl.create 16

let report name detail =
  Hashtbl.replace failures name (1 + try Hashtbl.find failures name with Not_found -> 0) ;
  if not (Hashtbl.mem shown name) then begin
    Hashtbl.replace shown name () ;
    Printf.printf "\n*** %s ***\n%s\n" name detail
  end


let short s = if String.length s > 70 then String.sub s 0 67 ^ "..." else s

let pp_t t =
  let b = Buffer.create 64 in
  let f = Format.formatter_of_buffer b in
  T.pp (fun f v -> Format.fprintf f "x%d" v) f t ;
  Format.pp_print_flush f () ;
  short (Buffer.contents b)


let pp_a a =
  let b = Buffer.create 64 in
  let f = Format.formatter_of_buffer b in
  A.pp_with_pp_var (fun f v -> Format.fprintf f "x%d" v) f a ;
  Format.pp_print_flush f () ;
  short (Buffer.contents b)


let seed = try int_of_string Sys.argv.(1) with _ -> 20261116

let iters = try int_of_string Sys.argv.(2) with _ -> 20000

let st = Random.State.make [|seed|]

(* ------------------------------------------------------------------ generators *)
let consts =
  [| Q.zero
   ; Q.one
   ; Q.minus_one
   ; Q.of_int 2
   ; Q.of_int (-3)
   ; Q.of_int 10
   ; Q.of_int 1000
   ; Q.make (Z.of_int 1) (Z.of_int 2)
   ; Q.make (Z.of_int (-7)) (Z.of_int 3)
   ; Q.of_float 0.1
   ; Q.of_bigint (Z.of_string "9223372036854775808") |]


let gen_q () = consts.(Random.State.int st (Array.length consts))

let gen_var () = Random.State.int st 4

let rec gen_term d =
  if d <= 0 then if Random.State.int st 2 = 0 then T.Const (gen_q ()) else T.Var (gen_var ())
  else
    let a () = gen_term (d - 1) and b () = gen_term (d - 1) in
    match Random.State.int st 14 with
    | 0 ->
        T.Add (a (), b ())
    | 1 ->
        T.Mult (a (), b ())
    | 2 ->
        T.Minus (a ())
    | 3 ->
        T.DivI (a (), b ())
    | 4 ->
        T.DivF (a (), b ())
    | 5 ->
        T.Mod (a (), b ())
    | 6 ->
        T.LessThan (a (), b ())
    | 7 ->
        T.LessEqual (a (), b ())
    | 8 ->
        T.Equal (a (), b ())
    | 9 ->
        T.NotEqual (a (), b ())
    | 10 ->
        T.And (a (), b ())
    | 11 ->
        T.Or (a (), b ())
    | 12 ->
        T.Not (a ())
    | _ ->
        T.BitAnd (a (), b ())


let gen_atom () =
  let t1 = gen_term (1 + Random.State.int st 2) and t2 = gen_term (1 + Random.State.int st 2) in
  match Random.State.int st 4 with
  | 0 ->
      A.LessEqual (t1, t2)
  | 1 ->
      A.LessThan (t1, t2)
  | 2 ->
      A.Equal (t1, t2)
  | _ ->
      A.NotEqual (t1, t2)


(* Pulse passes a real "is this term known nonzero" oracle; a unit-test context has none, so
   the honest stand-in is the constant-false one (knows nothing). *)
let is_neq_zero _t = false

(* ------------------------------------------------------------------ the run *)
let () =
  Printf.printf "normalisation idempotence / order independence, verbatim frontier 1a41747f\n" ;
  Printf.printf "seed=%d iters=%d\n" seed iters ;
  for _ = 1 to iters do
    let t = gen_term (1 + Random.State.int st 3) in
    (* B1: eval_const_shallow is a fixpoint *)
    ( match T.eval_const_shallow t with
    | Unsat _ ->
        hit "B1 first eval was Unsat"
    | Sat t1 -> (
        if not (T.equal_syntax t t1) then hit "B1 first eval CHANGED the term" ;
        match T.eval_const_shallow t1 with
        | Unsat _ ->
            report "B1 eval_const_shallow: Sat then UNSAT on its own output"
              (Printf.sprintf "  t      = %s\n  eval t = %s\n  eval (eval t) = UNSAT" (pp_t t)
                 (pp_t t1) )
        | Sat t2 ->
            if not (T.equal_syntax t1 t2) then
              report "B1 eval_const_shallow is not idempotent"
                (Printf.sprintf "  t         = %s\n  eval t    = %s\n  eval(eval t) = %s" (pp_t t)
                   (pp_t t1) (pp_t t2) ) ) ) ;
    (* B2: simplify_shallow is a fixpoint *)
    ( match T.simplify_shallow t with
    | Unsat _ ->
        hit "B2 first simplify was Unsat"
    | Sat s1 -> (
        if not (T.equal_syntax t s1) then hit "B2 first simplify CHANGED the term" ;
        match T.simplify_shallow s1 with
        | Unsat _ ->
            report "B2 simplify_shallow: Sat then UNSAT on its own output"
              (Printf.sprintf "  t          = %s\n  simp t     = %s\n  simp(simp t) = UNSAT"
                 (pp_t t) (pp_t s1) )
        | Sat s2 ->
            if not (T.equal_syntax s1 s2) then
              report "B2 simplify_shallow is not idempotent"
                (Printf.sprintf "  t            = %s\n  simp t       = %s\n  simp(simp t) = %s"
                   (pp_t t) (pp_t s1) (pp_t s2) ) ) ) ;
    (* E1: commutative operators must fold the same both ways *)
    (* E1 operands lean CONSTANT on purpose: with random deep terms both sides almost never
       fold to constants, so the property never reaches its condition and reports a pass.
       The mutation control caught exactly that. *)
    let gen_e1 () = if Random.State.int st 10 < 8 then T.Const (gen_q ()) else gen_term 1 in
    let a = gen_e1 () and b = gen_e1 () in
    List.iter
      (fun (nm, mk) ->
        let l = T.eval_const_shallow (mk a b) and r = T.eval_const_shallow (mk b a) in
        match (l, r) with
        | Sat (T.Const q1), Sat (T.Const q2) ->
            hit "E1 both sides folded to constants" ;
            if not (Q.equal q1 q2) then
              report
                ("E1 " ^ nm ^ " is not commutative after folding")
                (Printf.sprintf "  a = %s\n  b = %s\n  %s(a,b) = %s\n  %s(b,a) = %s" (pp_t a)
                   (pp_t b) nm (Q.to_string q1) nm (Q.to_string q2) )
        | Unsat _, Sat (T.Const _) | Sat (T.Const _), Unsat _ ->
            report
              ("E1 " ^ nm ^ ": one argument order is UNSAT and the other folds")
              (Printf.sprintf "  a = %s\n  b = %s" (pp_t a) (pp_t b))
        | _ ->
            () )
      [ ("Add", fun x y -> T.Add (x, y))
      ; ("Mult", fun x y -> T.Mult (x, y))
      ; ("Equal", fun x y -> T.Equal (x, y))
      ; ("NotEqual", fun x y -> T.NotEqual (x, y))
      ; ("And", fun x y -> T.And (x, y))
      ; ("Or", fun x y -> T.Or (x, y)) ] ;
    (* B3: Atom.eval is a fixpoint; B4: nnot is an involution *)
    let at = gen_atom () in
    if not (aeq (A.nnot (A.nnot at)) at) then
      report "B4 Atom.nnot is not an involution" ("  a = " ^ pp_a at) ;
    match A.eval ~is_neq_zero at with
    | Unsat _ ->
        hit "B3 first Atom.eval was Unsat"
    | Sat atoms ->
        if List.length atoms > 1 then hit "B3 Atom.eval returned several atoms" ;
        if not (match atoms with [a1] -> aeq a1 at | _ -> false) then
          hit "B3 first Atom.eval CHANGED the atom" ;
        List.iter
          (fun a1 ->
            match A.eval ~is_neq_zero a1 with
            | Unsat _ ->
                report "B3 Atom.eval: Sat then UNSAT on its own output"
                  (Printf.sprintf "  a       = %s\n  eval a  = %s\n  eval(eval a) = UNSAT" (pp_a at)
                     (pp_a a1) )
            | Sat [a2] ->
                if not (aeq a1 a2) then
                  report "B3 Atom.eval is not idempotent"
                    (Printf.sprintf "  a            = %s\n  eval a       = %s\n  eval(eval a) = %s"
                       (pp_a at) (pp_a a1) (pp_a a2) )
            | Sat others ->
                report "B3 Atom.eval on its own output returned a different atom count"
                  (Printf.sprintf "  a      = %s\n  eval a = %s\n  again  = [%s]" (pp_a at)
                     (pp_a a1)
                     (String.concat "; " (List.map pp_a others)) ) )
          atoms
  done ;
  print_endline
    "\n==== REACH (a fixpoint property over inputs that are already fixpoints is vacuous) ====" ;
  let ks = Hashtbl.fold (fun k v acc -> (k, v) :: acc) reach [] in
  List.iter
    (fun (k, v) -> Printf.printf "  %7d  %s\n" v k)
    (List.sort (fun (_, a) (_, b) -> compare b a) ks) ;
  print_endline "\n==== RESULTS ====" ;
  if Hashtbl.length failures = 0 then print_endline "  no property violations"
  else
    let fs = Hashtbl.fold (fun k v acc -> (k, v) :: acc) failures [] in
    List.iter
      (fun (k, v) -> Printf.printf "  %7d  %s\n" v k)
      (List.sort (fun (_, a) (_, b) -> compare b a) fs)
