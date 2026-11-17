(* shim of infer/src/pulse/PulseSatUnsat.ml, matching the real structure: the Unsat payload is a
   NAMED record type [unsat_info], not an inlined one, so it can be bound and passed to
   [log_unsat] (PulseFormulaLinArit.ml:222). *)
module Types = struct
  type unsat_info = {reason: unit -> string; source: string * int * int * int}

  type 'a sat_unsat_t = Unsat of unsat_info | Sat of 'a
end

include Types

type nonrec 'a t = 'a sat_unsat_t = Unsat of unsat_info | Sat of 'a

let log_unsat _unsat_info = ()

let map f = function Unsat u -> Unsat u | Sat x -> Sat (f x)

let bind f = function Unsat u -> Unsat u | Sat x -> f x

let sat = function
  | Unsat u ->
      log_unsat u ;
      None
  | Sat x ->
      Some x


let of_option unsat_info = function None -> Unsat unsat_info | Some x -> Sat x

(* same semantics as the real list_fold: stop at the first Unsat and propagate it *)
let list_fold l ~init ~f =
  let rec go acc = function
    | [] ->
        Sat acc
    | x :: tl -> (
      match f acc x with Unsat u -> Unsat u | Sat acc' -> go acc' tl )
  in
  go init l


let seq_fold seq ~init ~f =
  Stdlib.Seq.fold_left
    (fun acc x -> match acc with Unsat u -> Unsat u | Sat a -> f a x)
    (Sat init) seq


let to_list sat_unsat = match sat_unsat with Sat x -> [x] | Unsat _ -> []

let filter l = Stdlib.List.filter_map (function Sat x -> Some x | Unsat _ -> None) l

module Import = struct
  include Types

  let ( >>| ) x f = map f x

  let ( >>= ) x f = bind f x

  let ( let+ ) x f = map f x

  let ( let* ) x f = bind f x
end
