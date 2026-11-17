module MakePPMap (Ord : sig
  type t

  val compare : t -> t -> int

  val pp : Format.formatter -> t -> unit
end) =
struct
  include Stdlib.Map.Make (struct
    type t = Ord.t

    let compare = Ord.compare
  end)

  let is_singleton_or_more m =
    match bindings m with
    | [] ->
        IContainer.Empty
    | [x] ->
        IContainer.Singleton x
    | _ ->
        IContainer.More


  let pp ~pp_value fmt m = iter (fun k v -> Format.fprintf fmt "%a->%a " Ord.pp k pp_value v) m
end
