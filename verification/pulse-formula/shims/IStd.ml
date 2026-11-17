(* Minimal IStd shim. infer's real IStd re-exports Core; the verbatim files need (a) the
   labelled-argument Option/List/Int modules and (b) the compare_/equal_/yojson_of_ helpers
   that ppx_compare and ppx_yojson_conv expect to find in scope. *)
module Option = Base.Option
module List = Base.List
module Int = Base.Int
module Bool = Base.Bool
module String = Base.String
module Container = Base.Container
module Int64 = Base.Int64

let phys_equal = Base.phys_equal

(* ppx support, same names Base exposes *)
let compare_string = Base.compare_string

let equal_string = Base.equal_string

let yojson_of_string s = `String s

let compare_int = Base.compare_int

let equal_int = Base.equal_int

let yojson_of_int i = `Int i

let compare_bool = Base.compare_bool

let equal_bool = Base.equal_bool

let yojson_of_bool b = `Bool b

let compare_float = Base.compare_float

let equal_float = Base.equal_float

let yojson_of_float f = `Float f

let compare_list = Base.compare_list

let equal_list = Base.equal_list

let yojson_of_list f l = `List (Base.List.map l ~f)

let compare_option = Base.compare_option

let equal_option = Base.equal_option

let yojson_of_option f = function None -> `Null | Some x -> f x
