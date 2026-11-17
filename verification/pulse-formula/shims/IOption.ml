module Let_syntax = struct
  let ( let* ) x f = match x with None -> None | Some v -> f v

  let ( let+ ) x f = match x with None -> None | Some v -> Some (f v)

  let ( >>= ) x f = match x with None -> None | Some v -> f v

  let ( >>| ) x f = match x with None -> None | Some v -> Some (f v)

  let return x = Some x
end
