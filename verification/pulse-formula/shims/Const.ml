open! IStd

type t =
  | Cint of IntLit.t
  | Cfloat of float
  | Cstr of string
  | Cfun of Procname.t
  | Cclass of string

let compare = Stdlib.compare

let equal (a : t) b = Stdlib.( = ) a b

let yojson_of_t = function
  | Cint i ->
      `String (Z.to_string i)
  | Cfloat f ->
      `Float f
  | Cstr s ->
      `String s
  | Cfun p ->
      `String p
  | Cclass c ->
      `String c


let pp _pp fmt = function
  | Cint i ->
      Z.pp_print fmt i
  | Cfloat f ->
      Format.fprintf fmt "%f" f
  | Cstr s ->
      Format.fprintf fmt "%S" s
  | Cfun p ->
      Format.pp_print_string fmt p
  | Cclass c ->
      Format.pp_print_string fmt c
