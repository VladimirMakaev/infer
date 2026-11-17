(* shim: infer's PolyVariantEqual re-enables polymorphic (=) for argument-free variants only *)
let ( = ) (a : [> ]) b = Stdlib.( = ) a b

let ( <> ) (a : [> ]) b = Stdlib.( <> ) a b
