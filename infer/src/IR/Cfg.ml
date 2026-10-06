(*
 * Copyright (c) 2009-2013, Monoidics ltd.
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

open! IStd
module L = Logging
module F = Format

(** data type for the control flow graph *)
type t = Procdesc.t Procname.Hash.t

let create () = Procname.Hash.create 16

let get_sorted_procs cfg =
  let compare_proc_desc_by_proc_name pdesc1 pdesc2 =
    Procname.compare (Procdesc.get_proc_name pdesc1) (Procdesc.get_proc_name pdesc2)
  in
  Procname.Hash.fold (fun _ pdesc acc -> pdesc :: acc) cfg []
  |> List.sort ~compare:compare_proc_desc_by_proc_name


let get_all_defined_proc_names cfg =
  let procs = ref [] in
  let f pname pdesc = if Procdesc.is_defined pdesc then procs := pname :: !procs in
  Procname.Hash.iter f cfg ;
  !procs


let create_proc_desc cfg (proc_attributes : ProcAttributes.t) =
  let pdesc = Procdesc.from_proc_attributes proc_attributes in
  let pname = proc_attributes.proc_name in
  if Procname.Hash.mem cfg pname then
    L.die InternalError "Creating two procdescs for the same procname." ;
  Procname.Hash.add cfg pname pdesc ;
  pdesc


let iter_sorted cfg ~f = get_sorted_procs cfg |> List.iter ~f

let fold_sorted cfg ~init ~f = get_sorted_procs cfg |> List.fold ~init ~f

let store source_file cfg =
  let duplicate_definitions = ref 0 in
  (* Only one definition per procedure name is kept, so a non-static C/C++ function defined in
     several files of the capture loses all but one of them. Definitions with the same location,
     eg inline functions of a header, are the same code. *)
  let check_stored_definition (attributes : ProcAttributes.t) (stored : ProcAttributes.t) =
    if
      (not (SourceFile.equal stored.translation_unit source_file))
      && not (SourceFile.equal stored.loc.file attributes.loc.file)
    then (
      incr duplicate_definitions ;
      L.user_warning "%a is defined in both %a and %a; only one of the definitions is analyzed@\n"
        Procname.pp attributes.proc_name SourceFile.pp stored.loc.file SourceFile.pp
        attributes.loc.file )
  in
  let save_proc pname proc_desc =
    let attributes = Procdesc.get_attributes proc_desc in
    let loc = attributes.loc in
    let attributes' =
      let loc' = if Location.is_dummy loc then {loc with file= source_file} else loc in
      {attributes with loc= loc'; translation_unit= source_file}
    in
    Procdesc.set_attributes proc_desc attributes' ;
    let check_stored_definition =
      Option.some_if
        (attributes.is_defined && Language.equal (Procname.get_language pname) Clang)
        (check_stored_definition attributes')
    in
    Attributes.store ?check_stored_definition
      ~proc_desc:(Option.some_if attributes.is_defined proc_desc)
      attributes' ~analysis:false
  in
  Procname.Hash.iter save_proc cfg ;
  if !duplicate_definitions > 0 then
    StatsLogging.log_count ~label:"capture_duplicate_definitions" ~value:!duplicate_definitions


let pp_proc_signatures fmt cfg =
  F.fprintf fmt "@[<v>METHOD SIGNATURES@;" ;
  iter_sorted ~f:(Procdesc.pp_signature fmt) cfg ;
  F.fprintf fmt "@]"
