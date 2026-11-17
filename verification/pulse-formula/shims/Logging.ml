(* shim. The ?color annotation is load-bearing: the verbatim code writes `~color:Orange`
   unqualified and relies on type-directed disambiguation against Pp.color. *)
let sink = Format.make_formatter (fun _ _ _ -> ()) (fun () -> ())

let d_printf fmt = Format.ifprintf sink fmt

let d_printfln ?(color : Pp.color option) fmt =
  ignore color ;
  Format.ifprintf sink fmt


let die fmt = Format.kasprintf failwith fmt
