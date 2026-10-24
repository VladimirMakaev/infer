(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)
open! IStd
module F = Format

type t =
  | Call of Procname.t
  | Model of string
  | ModelName of Procname.t
  | SkippedKnownCall of Procname.t
  | SkippedUnknownCall of Exp.t
[@@deriving compare, equal, yojson_of]

let pp_config ~verbose fmt =
  let pp_proc_name proc_name =
    if verbose then if Procname.is_swift proc_name then Procname.pp_fullname_only else Procname.pp
    else Procname.describe
  in
  (* the clang frontend wraps the initialiser of a global in a procedure of its own, whose
     mangled name means nothing to a reader: name the global instead *)
  let global_of_initializer proc_name =
    if verbose then None else Procname.get_global_name_of_initializer proc_name
  in
  function
  | Call proc_name -> (
    match global_of_initializer proc_name with
    | Some global ->
        F.fprintf fmt "the initializer of `%s`" global
    | None ->
        F.fprintf fmt "`%a`" (pp_proc_name proc_name) proc_name )
  | Model model ->
      F.fprintf fmt "`%s` (modelled)" model
  | ModelName proc_name ->
      F.fprintf fmt "`%a` (modelled)" (pp_proc_name proc_name) proc_name
  | SkippedKnownCall proc_name -> (
    match global_of_initializer proc_name with
    | Some global ->
        F.fprintf fmt "the initializer of `%s` with no summary" global
    | None ->
        F.fprintf fmt "function `%a` with no summary" (pp_proc_name proc_name) proc_name )
  | SkippedUnknownCall call_exp ->
      F.fprintf fmt "unresolved call expression `%a`" Exp.pp call_exp


let pp = pp_config ~verbose:true

let describe = pp_config ~verbose:false

let pp_name_only ~with_class fmt = function
  | Call proc_name | ModelName proc_name | SkippedKnownCall proc_name ->
      if with_class then Procname.pp_without_templates fmt proc_name
      else F.pp_print_string fmt (Procname.get_method proc_name)
  | Model model ->
      F.fprintf fmt "%s" model
  | SkippedUnknownCall call_exp ->
      Exp.pp fmt call_exp


let to_name_only call_event = F.asprintf "%a" (pp_name_only ~with_class:true) call_event
