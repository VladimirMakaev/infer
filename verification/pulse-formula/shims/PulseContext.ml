(* Term calls this for is_int range reasoning; None is the "no widths known" case the real
   code already handles, and is what a unit-test context gives. *)
let integer_widths () : IntegerWidths.t option = None
