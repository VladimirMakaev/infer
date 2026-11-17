(* Did the big-coefficient arm actually reach QSafeCapped's cap? An oracle that never reaches
   the condition it is testing reports a clean run for the wrong reason, so measure it. *)

module L = PulseFormulaLinArit
module QC = QSafeCapped

let is_rational q = match Q.classify q with Q.ZERO | Q.NZERO -> true | _ -> false

let show q = if is_rational q then Q.to_string q else "UNDEF"

let pp_l l =
  let b = Buffer.create 64 in
  let fmt = Format.formatter_of_buffer b in
  L.pp (fun fmt v -> Format.fprintf fmt "x%d" v) fmt l ;
  Format.pp_print_flush fmt () ;
  Buffer.contents b


let () =
  (* 1. the cap itself: QSafeCapped.cap poisons above 128 bits of num or den *)
  let huge = Q.make (Z.pow (Z.of_int 10) 30) Z.one in
  print_endline "== 1. QSafeCapped.mul on large rationals ==" ;
  Printf.printf "  huge                = %s (num bits %d)\n" (Q.to_string huge)
    (Z.numbits (Q.num huge)) ;
  Printf.printf "  QC.mul huge huge    = %s\n" (show (QC.mul huge huge)) ;
  Printf.printf "  Q.mul  huge huge    = %s   <- plain zarith, for contrast\n"
    (Q.to_string (Q.mul huge huge)) ;
  Printf.printf "  QC.add huge huge    = %s   <- add is NOT capped\n" (show (QC.add huge huge)) ;
  Printf.printf "  QC.div huge (1/huge)= %s   <- div is NOT capped\n"
    (show (QC.div huge (Q.inv huge))) ;
  (* 2. can a poisoned coefficient get inside a LinArith term through the public API? *)
  print_endline "\n== 2. LinArith.mult with a large coefficient ==" ;
  let l = L.mult huge (L.of_var 0) in
  let l2 = L.mult huge l in
  Printf.printf "  huge*x0             = %s\n" (pp_l l) ;
  Printf.printf "  huge*(huge*x0)      = %s\n" (pp_l l2) ;
  let poisoned_coeff = L.fold l2 ~init:false ~f:(fun acc (_, q) -> acc || not (is_rational q)) in
  Printf.printf "  carries a non-rational coefficient? %b\n" poisoned_coeff ;
  (* 3. if so, what do the downstream query functions say about it? *)
  if poisoned_coeff then begin
    print_endline "\n== 3. downstream queries on a poisoned linear form ==" ;
    Printf.printf "  is_zero            = %b\n" (L.is_zero l2) ;
    Printf.printf "  get_as_const       = %s\n"
      (match L.get_as_const l2 with None -> "None" | Some c -> "Some " ^ show c) ;
    Printf.printf "  get_as_var         = %s\n"
      (match L.get_as_var l2 with None -> "None" | Some v -> Printf.sprintf "Some x%d" v) ;
    Printf.printf "  get_constant_part  = %s\n" (show (L.get_constant_part l2)) ;
    Printf.printf "  classify           = %s\n"
      ( match L.classify_minimized_maximized l2 with
      | `Minimized ->
          "`Minimized"
      | `Maximized ->
          "`Maximized"
      | `Neither ->
          "`Neither"
      | `Constant ->
          "`Constant" ) ;
    (* the one that matters: does the solver draw a conclusion from a poisoned form? *)
    match L.solve_eq l2 (L.of_q Q.zero) with
    | Sat None ->
        print_endline "  solve_eq l2 = 0   -> Sat None  (ALWAYS TRUE)"
    | Sat (Some (x, sol)) ->
        Printf.printf "  solve_eq l2 = 0   -> Sat (x%d = %s)\n" x (pp_l sol)
    | Unsat _ ->
        print_endline "  solve_eq l2 = 0   -> UNSAT"
  end ;
  (* 4. the same question for the subtraction of two poisoned-but-equal forms *)
  print_endline "\n== 4. does a poisoned coefficient defeat x - x = 0 ? ==" ;
  let d = L.subtract l2 l2 in
  Printf.printf "  l2 - l2            = %s\n" (pp_l d) ;
  Printf.printf "  is_zero (l2 - l2)  = %b   <- should be true\n" (L.is_zero d) ;
  match L.solve_eq l2 l2 with
  | Sat None ->
      print_endline "  solve_eq l2 l2     = Sat None (always true)  <- correct"
  | Sat (Some (x, sol)) ->
      Printf.printf "  solve_eq l2 l2     = Sat (x%d = %s)\n" x (pp_l sol)
  | Unsat _ ->
      print_endline "  solve_eq l2 l2     = UNSAT   <- WRONG: l = l is a tautology"
