(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)
open! IStd
module F = Format

type skipped_call_cause = NoSummary | ArityMismatch | SummaryNotApplicable | ForceContinue
[@@deriving compare, equal, yojson_of]

type t =
  | Call of Procname.t
  | Model of string
  | ModelName of Procname.t
  | SkippedKnownCall of Procname.t * skipped_call_cause
  | SkippedUnknownCall of Exp.t
[@@deriving compare, equal, yojson_of]

let pp_config ~verbose ~show_skipped_call_cause fmt =
  let pp_proc_name proc_name =
    if verbose then if Procname.is_swift proc_name then Procname.pp_fullname_only else Procname.pp
    else Procname.describe
  in
  (* the clang frontend wraps the initialiser of a global in a procedure of its own, whose mangled
     name means nothing to a reader: name the global instead. The global determines the procedure,
     so this loses nothing even where the rest of the name is printed in full. *)
  function
  | Call proc_name -> (
    match Procname.get_global_name_of_initializer proc_name with
    | Some global ->
        F.fprintf fmt "the initializer of `%s`" global
    | None ->
        F.fprintf fmt "`%a`" (pp_proc_name proc_name) proc_name )
  | Model model ->
      F.fprintf fmt "`%s` (modelled)" model
  | ModelName proc_name ->
      F.fprintf fmt "`%a` (modelled)" (pp_proc_name proc_name) proc_name
  | SkippedKnownCall (proc_name, cause) ->
      let cause = if show_skipped_call_cause then cause else NoSummary in
      ( match Procname.get_global_name_of_initializer proc_name with
      | Some global ->
          F.fprintf fmt "the initializer of `%s`" global
      | None ->
          F.fprintf fmt "function `%a`" (pp_proc_name proc_name) proc_name ) ;
      F.pp_print_string fmt
        ( match cause with
        | NoSummary ->
            " with no summary"
        | ArityMismatch ->
            " whose argument count does not match its definition"
        | SummaryNotApplicable ->
            " whose summary could not be applied"
        | ForceContinue ->
            " with no applicable normal postcondition (forced continuation)" )
  | SkippedUnknownCall call_exp ->
      F.fprintf fmt "unresolved call expression `%a`" Exp.pp call_exp


let pp = pp_config ~verbose:true ~show_skipped_call_cause:true

let describe = pp_config ~verbose:false ~show_skipped_call_cause:false

let pp_for_report = pp_config ~verbose:true ~show_skipped_call_cause:false

let pp_name_only ~with_class fmt = function
  | Call proc_name | ModelName proc_name | SkippedKnownCall (proc_name, _) ->
      if with_class then Procname.pp_without_templates fmt proc_name
      else F.pp_print_string fmt (Procname.get_method proc_name)
  | Model model ->
      F.fprintf fmt "%s" model
  | SkippedUnknownCall call_exp ->
      Exp.pp fmt call_exp


let to_name_only call_event = F.asprintf "%a" (pp_name_only ~with_class:true) call_event
