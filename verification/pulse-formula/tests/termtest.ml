(* Property/differential testing of the VERBATIM PulseFormulaTerm.ml (frontier 1a41747f).
 *
 * Oracle: an independent evaluator in PLAIN zarith Q/Z. It must not use QSafeCapped -- the
 * capped arithmetic is part of what is under test, and an oracle sharing it cannot see a
 * capping defect (that is how finding 3 stayed invisible on the first pass).
 *
 * Shape: eval_const_shallow folds a term whose DIRECT SUBTERMS are constants, so the
 * generator builds Op (Const a, Const b) and Op (Const a) and compares the fold against the
 * oracle. "fold-then-evaluate = evaluate-then-fold".
 *
 * Every interesting condition has a REACH COUNTER. A property that never reaches its
 * condition reports a pass, which is not the same as being correct.
 *)

module T = PulseFormulaTerm
module QC = QSafeCapped

(* ------------------------------------------------------------------ reach counters *)
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


(* ------------------------------------------------------------------ helpers *)
let is_rational q = match Q.classify q with Q.ZERO | Q.NZERO -> true | _ -> false

let is_int_q q = is_rational q && Z.equal (Q.den q) Z.one

let qs q = if is_rational q then Q.to_string q else "UNDEF"

let short s = if String.length s > 60 then String.sub s 0 57 ^ "..." else s

let pp_t t =
  let b = Buffer.create 64 in
  let fmt = Format.formatter_of_buffer b in
  T.pp (fun fmt v -> Format.fprintf fmt "x%d" v) fmt t ;
  Format.pp_print_flush fmt () ;
  short (Buffer.contents b)


(* what Term returned, as a comparable summary *)
type got = GConst of Q.t | GOther of string | GUnsat

let run t =
  match T.eval_const_shallow t with
  | Sat (T.Const q) ->
      GConst q
  | Sat t' ->
      GOther (pp_t t')
  | Unsat _ ->
      GUnsat


let gs = function GConst q -> "Const " ^ qs q | GOther s -> "term " ^ s | GUnsat -> "UNSAT"

(* oracle result *)
type want =
  | WConst of Q.t
  | WUndef (* genuinely undefined: must not produce a value *)
  | WSkip of string (* outside what the oracle claims to model *)

(* ------------------------------------------------------------------ the oracle *)
let fits_i64 q = is_int_q q && Z.fits_int64 (Q.num q)

let to_i64 q = Z.to_int64 (Q.num q)

(* NOTE: no `open Q` -- zarith's Q defines its own (=) and friends, which silently shadow
   the int comparisons used below. *)
let oracle_bin op (a : Q.t) (b : Q.t) : want =
  let zero_b = Int.equal (Q.sign b) 0 in
  match op with
  | `Add ->
      WConst (Q.add a b)
  | `Mult ->
      WConst (Q.mul a b) (* plain Q.mul: exact, never capped *)
  | `DivF ->
      if zero_b then WUndef else WConst (Q.div a b)
  | `DivI ->
      if zero_b then WUndef
      else
        (* truncate the exact quotient toward zero, as C integer division does *)
        let n = Z.mul (Q.num a) (Q.den b) and d = Z.mul (Q.den a) (Q.num b) in
        WConst (Q.of_bigint (Z.div n d))
  | `Mod ->
      if zero_b then WUndef
      else if is_int_q a && is_int_q b then WConst (Q.of_bigint (Z.rem (Q.num a) (Q.num b)))
      else WSkip "Mod with a non-integer operand"
  | `Lt ->
      WConst (if Q.lt a b then Q.one else Q.zero)
  | `Le ->
      WConst (if Q.leq a b then Q.one else Q.zero)
  | `Eq ->
      WConst (if Q.equal a b then Q.one else Q.zero)
  | `Ne ->
      WConst (if Q.equal a b then Q.zero else Q.one)
  | `And ->
      WConst (if (not (Int.equal (Q.sign a) 0)) && not zero_b then Q.one else Q.zero)
  | `Or ->
      WConst (if (not (Int.equal (Q.sign a) 0)) || not zero_b then Q.one else Q.zero)
  | `BAnd | `BOr | `BXor ->
      (* Pulse's integer domain is UNBOUNDED (that is the stated rationale for the frontier's
         IsInt change: "arithmetic is on Q"). The consistent spec for a bitwise op on integers
         is therefore arbitrary-precision Z.logand/logor/logxor, which is total on
         non-negative integers. Restricting the oracle to int64 would SKIP exactly the cases
         where the implementation's int64 round-trip can fail -- the skip that hid this. *)
      if not (is_int_q a && is_int_q b) then WSkip "bitwise operand is not an integer"
      else if Z.sign (Q.num a) < 0 || Z.sign (Q.num b) < 0 then
        WSkip "negative bitwise operand (two's-complement width is ambiguous unbounded)"
      else
        let f = match op with `BAnd -> Z.logand | `BOr -> Z.logor | _ -> Z.logxor in
        WConst (Q.of_bigint (f (Q.num a) (Q.num b)))
  | `Shl | `Shr ->
      if not (fits_i64 a) then WSkip "shift value outside int64"
      else if not (is_int_q b) then WSkip "non-integer shift amount"
      else
        let n = Q.num b in
        if Z.lt n Z.zero || Z.gt n (Z.of_int 63) then WSkip "shift amount outside [0,63]"
        else
          let k = Z.to_int n in
          let f = match op with `Shl -> Int64.shift_left | _ -> Int64.shift_right in
          WConst (Q.of_bigint (Z.of_int64 (f (to_i64 a) k)))


let mk_bin op a b =
  let ca = T.Const a and cb = T.Const b in
  match op with
  | `Add ->
      T.Add (ca, cb)
  | `Mult ->
      T.Mult (ca, cb)
  | `DivI ->
      T.DivI (ca, cb)
  | `DivF ->
      T.DivF (ca, cb)
  | `Mod ->
      T.Mod (ca, cb)
  | `Lt ->
      T.LessThan (ca, cb)
  | `Le ->
      T.LessEqual (ca, cb)
  | `Eq ->
      T.Equal (ca, cb)
  | `Ne ->
      T.NotEqual (ca, cb)
  | `And ->
      T.And (ca, cb)
  | `Or ->
      T.Or (ca, cb)
  | `BAnd ->
      T.BitAnd (ca, cb)
  | `BOr ->
      T.BitOr (ca, cb)
  | `BXor ->
      T.BitXor (ca, cb)
  | `Shl ->
      T.BitShiftLeft (ca, cb)
  | `Shr ->
      T.BitShiftRight (ca, cb)


let op_name = function
  | `Add ->
      "Add"
  | `Mult ->
      "Mult"
  | `DivI ->
      "DivI"
  | `DivF ->
      "DivF"
  | `Mod ->
      "Mod"
  | `Lt ->
      "LessThan"
  | `Le ->
      "LessEqual"
  | `Eq ->
      "Equal"
  | `Ne ->
      "NotEqual"
  | `And ->
      "And"
  | `Or ->
      "Or"
  | `BAnd ->
      "BitAnd"
  | `BOr ->
      "BitOr"
  | `BXor ->
      "BitXor"
  | `Shl ->
      "BitShiftLeft"
  | `Shr ->
      "BitShiftRight"


let all_ops =
  [`Add; `Mult; `DivI; `DivF; `Mod; `Lt; `Le; `Eq; `Ne; `And; `Or; `BAnd; `BOr; `BXor; `Shl; `Shr]


(* ------------------------------------------------------------------ generators *)
let seed = try int_of_string Sys.argv.(1) with _ -> 20261116

let iters = try int_of_string Sys.argv.(2) with _ -> 40000

let st = Random.State.make [|seed|]

let interesting =
  [| Q.zero
   ; Q.one
   ; Q.minus_one
   ; Q.of_int 2
   ; Q.of_int (-2)
   ; Q.of_int 3
   ; Q.of_int 7
   ; Q.of_int 64
   ; Q.of_int 63
   ; Q.of_int 65
   ; Q.of_int (-1)
   ; Q.of_int 100
   ; Q.make (Z.of_int 1) (Z.of_int 2)
   ; (* 1/2 : the 30de9be354b shape *)
     Q.make (Z.of_int (-7)) (Z.of_int 2)
   ; Q.of_float 0.1
   ; Q.of_bigint (Z.of_string "9223372036854775807")
   ; (* Int64.max *)
     Q.of_bigint (Z.of_string "-9223372036854775808")
   ; (* Int64.min *)
     Q.of_bigint (Z.of_string "9223372036854775808")
   ; (* Int64.max + 1 *)
     Q.of_bigint (Z.pow (Z.of_int 10) 40)
   ; (* past the 128-bit cap *)
     Q.of_float 1e300 |]


let gen_q () =
  if Random.State.int st 100 < 70 then interesting.(Random.State.int st (Array.length interesting))
  else
    let n = Random.State.int st 41 - 20 and d = 1 + Random.State.int st 4 in
    Q.make (Z.of_int n) (Z.of_int d)


(* ------------------------------------------------------------------ the run *)
let () =
  Printf.printf "PulseFormulaTerm.eval_const_shallow, verbatim frontier 1a41747f\n" ;
  Printf.printf "seed=%d iters=%d\n" seed iters ;
  for _ = 1 to iters do
    let op = List.nth all_ops (Random.State.int st (List.length all_ops)) in
    let a = gen_q () and b = gen_q () in
    (* reach counters for the conditions the properties care about *)
    if not (is_rational a && is_rational b) then hit "operand already non-rational" ;
    if (match op with `BAnd | `BOr | `BXor -> true | _ -> false) && not (fits_i64 a && fits_i64 b)
    then hit "bitwise operand outside int64" ;
    if
      (match op with `Shl | `Shr -> true | _ -> false)
      && is_int_q b
      && (Z.lt (Q.num b) Z.zero || Z.gt (Q.num b) (Z.of_int 63))
    then hit "shift amount outside [0,63]" ;
    if
      (match op with `Mod -> true | _ -> false)
      && ((not (is_int_q a)) || not (is_int_q b))
      && not (Int.equal (Q.sign b) 0)
    then hit "Mod with a non-integer operand" ;
    if (match op with `DivI | `DivF | `Mod -> true | _ -> false) && Int.equal (Q.sign b) 0 then
      hit "division by zero" ;
    let t = mk_bin op a b in
    let got = run t in
    ( match got with
    | GConst q when not (is_rational q) ->
        hit "fold produced a non-rational"
    | GUnsat ->
        hit "fold returned Unsat"
    | _ ->
        () ) ;
    match oracle_bin op a b with
    | WSkip why ->
        hit ("skipped: " ^ why)
    | WUndef -> (
      match got with
      | GConst q ->
          report
            (op_name op ^ ": folded an UNDEFINED operation to a value")
            (Printf.sprintf "  %s(%s, %s)\n  oracle: undefined\n  term  : Const %s" (op_name op)
               (qs a) (qs b) (qs q) )
      | _ ->
          () )
    | WConst want -> (
      match got with
      | GConst q when Q.equal q want ->
          ()
      | GConst q ->
          report
            (op_name op ^ ": folded value disagrees with the oracle")
            (Printf.sprintf "  %s(%s, %s)\n  oracle: %s\n  term  : %s" (op_name op) (qs a) (qs b)
               (qs want) (qs q) )
      | GUnsat ->
          report
            (op_name op ^ ": returned UNSAT for a well-defined operation")
            (Printf.sprintf "  %s(%s, %s)\n  oracle: %s\n  term  : UNSAT (path dropped)"
               (op_name op) (qs a) (qs b) (qs want) )
      | GOther s ->
          (* leaving the term unfolded is sound: it just declines to compute *)
          ignore s ;
          hit "left unfolded (sound abstention)" )
  done ;
  print_endline "\n==== REACH (a property that never reaches its condition reports a pass) ====" ;
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
