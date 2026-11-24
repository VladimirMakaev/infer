(* Four independent bindings: machine-int operators and the actual Zarith module. *)
module Z = Z

let int_div a b = a / b

let int_rem a b = a mod b

let zarith_div = Z.div

let zarith_rem = Z.rem
