(* Var shim. LinArit names exactly: t, equal, compare_unrestricted_first, is_restricted,
   is_unrestricted, pp, yojson_of_t, Map.
   Restricted variables model Pulse's non-negative ("v_") variables: infer makes a variable
   restricted by a bit on the abstract value id. Here: ODD ids are restricted, even unrestricted.
   That reproduces the only property LinArit depends on -- a stable partition plus an order that
   puts unrestricted first. *)
(* ppx_compare expects these in scope, as Base would provide *)
let compare_int = Stdlib.compare

let equal_int (a : int) b = a = b

let yojson_of_int i = `Int i

type t = int [@@deriving compare, equal, yojson_of]

let is_restricted v = v land 1 = 1

let is_unrestricted v = not (is_restricted v)

let compare_unrestricted_first v1 v2 =
  match (is_unrestricted v1, is_unrestricted v2) with
  | true, false ->
      -1
  | false, true ->
      1
  | _ ->
      Int.compare v1 v2


let is_simpler_than v1 v2 = compare v1 v2 < 0

let pp fmt v = Format.fprintf fmt "%s%d" (if is_restricted v then "v" else "x") v

module Map = Stdlib.Map.Make (struct
  type nonrec t = t

  let compare = compare
end)

module Set = struct
  include Stdlib.Set.Make (struct
    type nonrec t = t

    let compare = compare
  end)

  let pp fmt s = iter (fun v -> Format.fprintf fmt "%a " pp v) s
end
