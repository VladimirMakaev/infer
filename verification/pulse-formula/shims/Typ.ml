open! IStd

type ikind =
  | IChar
  | ISChar
  | IUChar
  | IBool
  | IInt
  | IUInt
  | IShort
  | IUShort
  | ILong
  | IULong
  | ILongLong
  | IULongLong
  | I128
  | IU128

let compare_ikind = Stdlib.compare

let equal_ikind (a : ikind) b = Stdlib.( = ) a b

type t = {desc: string}

let compare = Stdlib.compare

let equal (a : t) b = Stdlib.( = ) a b

let ikind_to_string = function
  | IChar ->
      "char"
  | ISChar ->
      "signed char"
  | IUChar ->
      "unsigned char"
  | IBool ->
      "_Bool"
  | IInt ->
      "int"
  | IUInt ->
      "unsigned int"
  | IShort ->
      "short"
  | IUShort ->
      "unsigned short"
  | ILong ->
      "long"
  | IULong ->
      "unsigned long"
  | ILongLong ->
      "long long"
  | IULongLong ->
      "unsigned long long"
  | I128 ->
      "__int128"
  | IU128 ->
      "unsigned __int128"


let pp_full _pp fmt t = Format.pp_print_string fmt t.desc

let yojson_of_t t = `String t.desc

let yojson_of_ikind k = `String (ikind_to_string k)
