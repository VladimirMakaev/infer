open! IStd

type t = string

let compare = compare_string

let equal = equal_string

let yojson_of_t = yojson_of_string

let pp_without_templates fmt p = Format.pp_print_string fmt p
