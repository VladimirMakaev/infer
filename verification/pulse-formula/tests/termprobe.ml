(* The three conditions termtest's oracle SKIPS are where the unexplored behaviour is.
   A skip is not a pass, so probe each one directly and judge the answer by hand.

   Of particular interest: BitShiftLeft/Right do
       map_i64_i c1 c2 Int64.shift_left        (PulseFormulaTerm.ml:612)
   where the shift amount is converted with Q.to_int and NEVER RANGE-CHECKED. OCaml's
   Int64.shift_left is documented as UNSPECIFIED when the amount is < 0 or > 63. *)

module T = PulseFormulaTerm

let is_rational q = match Q.classify q with Q.ZERO | Q.NZERO -> true | _ -> false

let qs q = if is_rational q then Q.to_string q else "UNDEF"

let short s = if String.length s > 42 then String.sub s 0 39 ^ "..." else s

let run t =
  match T.eval_const_shallow t with
  | Sat (T.Const q) ->
      "Const " ^ short (qs q)
  | Sat _ ->
      "unfolded (abstains)"
  | Unsat _ ->
      "UNSAT (path dropped)"


let q_of_int i = Q.of_int i

let big s = Q.of_bigint (Z.of_string s)

let row label t expect = Printf.printf "  %-46s -> %-30s   %s\n" label (run t) expect

let () =
  print_endline "== 1. SHIFT AMOUNT OUTSIDE [0,63]: OCaml Int64.shift_left is UNSPECIFIED there ==" ;
  print_endline "   (reference: 1 shifted left by n, computed exactly in Z)" ;
  List.iter
    (fun n ->
      let t = T.BitShiftLeft (T.Const (q_of_int 1), T.Const (q_of_int n)) in
      let exact = if n >= 0 && n < 1000 then Z.to_string (Z.shift_left Z.one n) else "huge" in
      Printf.printf "  1 << %-5d -> %-34s  exact (unbounded) = %s\n" n (run t) (short exact) )
    [0; 1; 31; 62; 63; 64; 65; 100; 1000; -1; -64] ;
  print_endline "\n== 2. SHIFT RIGHT, same question ==" ;
  List.iter
    (fun n ->
      let t = T.BitShiftRight (T.Const (q_of_int 256), T.Const (q_of_int n)) in
      Printf.printf "  256 >> %-5d -> %s\n" n (run t) )
    [0; 8; 63; 64; 65; -1] ;
  print_endline "\n== 3. BITWISE WITH AN OPERAND OUTSIDE int64 ==" ;
  row "BitAnd(2^70, 1)"
    (T.BitAnd (T.Const (big "1180591620717411303424"), T.Const Q.one))
    "Q.to_int64 fails -> Undefined -> Unsat" ;
  row "BitOr(Int64.max, 1)" (T.BitOr (T.Const (big "9223372036854775807"), T.Const Q.one)) "fits" ;
  row "BitOr(Int64.max+1, 1)"
    (T.BitOr (T.Const (big "9223372036854775808"), T.Const Q.one))
    "just past int64" ;
  print_endline "\n== 4. Mod / DivI WITH A NON-INTEGER OPERAND (Q.to_bigint TRUNCATES) ==" ;
  let half = Q.make (Z.of_int 5) (Z.of_int 2) in
  row "Mod(7, 5/2)   [truncates 5/2 to 2 -> 7 mod 2 = 1]"
    (T.Mod (T.Const (q_of_int 7), T.Const half))
    "exact 7 mod 2.5 would be 2" ;
  row "Mod(7/2, 2)   [truncates 7/2 to 3 -> 3 mod 2 = 1]"
    (T.Mod (T.Const (Q.make (Z.of_int 7) (Z.of_int 2)), T.Const (q_of_int 2)))
    "silent truncation of the dividend" ;
  row "Mod(7, 1/2)   [the 30de9be354b shape]"
    (T.Mod (T.Const (q_of_int 7), T.Const (Q.make Z.one (Z.of_int 2))))
    "1/2 truncates to 0 -> div by zero, protected" ;
  row "DivI(7, 5/2)" (T.DivI (T.Const (q_of_int 7), T.Const half)) "exact trunc(2.8) = 2" ;
  print_endline "\n== 5. CONTROLS: ordinary folds must still be right ==" ;
  row "Add(2,3)" (T.Add (T.Const (q_of_int 2), T.Const (q_of_int 3))) "5" ;
  row "Mult(6,7)" (T.Mult (T.Const (q_of_int 6), T.Const (q_of_int 7))) "42" ;
  row "DivI(-7,2)" (T.DivI (T.Const (q_of_int (-7)), T.Const (q_of_int 2))) "C truncation: -3" ;
  row "Mod(-7,2)" (T.Mod (T.Const (q_of_int (-7)), T.Const (q_of_int 2))) "C remainder: -1" ;
  row "BitAnd(10,9)"
    (T.BitAnd (T.Const (q_of_int 10), T.Const (q_of_int 9)))
    "8  (the d35a0975f9a shape)"
