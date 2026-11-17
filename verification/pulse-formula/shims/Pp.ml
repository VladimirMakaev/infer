(* shim: pretty-printing only, never under test. Real Pp.collection also takes ?filter. *)
type color = Black | Blue | Green | Orange | Red

let text = `Text

let collection ~sep ~fold ?filter pp_item fmt coll =
  ignore sep ;
  let keep x = match filter with None -> true | Some f -> f x in
  let first = ref true in
  ignore
    (fold coll ~init:() ~f:(fun () x ->
         if keep x then begin
           if !first then first := false else Format.pp_print_cut fmt () ;
           pp_item fmt x
         end ) )


let seq ?(sep = " ") pp fmt l =
  List.iteri
    (fun i x ->
      if i > 0 then Format.pp_print_string fmt sep ;
      pp fmt x )
    l
