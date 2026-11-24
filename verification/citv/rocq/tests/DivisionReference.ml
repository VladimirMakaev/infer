let () =
  for a = -9 to 9 do
    for b = -5 to 5 do
      if b <> 0 then (
        let q = DivisionBindings.int_div a b in
        let r = DivisionBindings.int_rem a b in
        let zq = DivisionBindings.zarith_div (Z.of_int a) (Z.of_int b) in
        let zr = DivisionBindings.zarith_rem (Z.of_int a) (Z.of_int b) in
        assert (Z.equal zq (Z.of_int q) && Z.equal zr (Z.of_int r)) ;
        Printf.printf "%d %d %d %d\n" a b q r )
    done
  done
