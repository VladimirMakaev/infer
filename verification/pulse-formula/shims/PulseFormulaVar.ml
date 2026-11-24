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

(* mk_fresh. PulseFormulaTerm.mk_opaque calls this when a payload exceeds the opaque caps: the
   payload is dropped and replaced by a fresh UNCONSTRAINED variable, so the value stays "something
   we cannot compute" while the term becomes atomic. In infer this is PulseAbstractValue.mk_fresh,
   an upward counter (restricted values there are the NEGATIVE ones, from mk_fresh_restricted).
   This shim models restrictedness by parity instead -- odd restricted, even unrestricted -- so
   the equivalent is an upward counter over EVEN ids.

   TWO PROPERTIES THIS HAS TO KEEP, both load-bearing for what the harness is testing:
   - UNRESTRICTED. A restricted stand-in would carry a non-negativity constraint the original
     payload never had, which is a soundness change, not a modelling detail.
   - NEVER COLLIDING, with each other or with a variable a test generated. The engine comment at
     the cap is explicit that a shared stand-in would make two unrelated uncomputable values equal
     to each other. The property tests draw variables from Random.State.int st 4 and st 6, i.e.
     ids 0..5, so the counter starts at 1000: far above them, and stepping by 2 keeps every id
     even. If a future test widens that range past 1000 the collision would be silent here, which
     is why the start is a named constant rather than a literal in the increment. *)
let first_fresh_var = 1000

let next_fresh = ref first_fresh_var

let mk_fresh () =
  let v = !next_fresh in
  next_fresh := v + 2 ;
  v


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
