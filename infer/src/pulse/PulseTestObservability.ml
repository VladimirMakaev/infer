(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

open! IStd

let location (loc : Location.t) =
  `Assoc
    [ ("file", `String (SourceFile.to_string ~force_relative:true loc.file))
    ; ("line", `Int loc.line)
    ; ("column", `Int loc.col) ]


let proc_fields proc_name =
  [ ("proc_uid", `String (Procname.to_unique_id proc_name))
  ; ("procedure", `String (Procname.to_string proc_name)) ]


let emit proc_name event fields =
  if Config.pulse_test_observability then
    StatsLogging.log_message ~label:"pulse_test_observation"
      ~message:
        (Yojson.Basic.to_string
           (`Assoc
              ( (("schema_version", `Int 1) :: ("event", `String event) :: proc_fields proc_name)
              @ fields ) ) )


let start proc_desc =
  if Config.pulse_test_observability then
    let calls =
      Procdesc.fold_instrs proc_desc ~init:[] ~f:(fun calls _node instr ->
          match instr with
          | Sil.Call (_, callee, _, loc, _) ->
              let callee =
                match callee with
                | Exp.Const (Const.Cfun proc_name) ->
                    `Assoc (proc_fields proc_name)
                | _ ->
                    `Null
              in
              `Assoc [("location", location loc); ("callee", callee)] :: calls
          | _ ->
              calls )
    in
    emit
      (Procdesc.get_proc_name proc_desc)
      "analysis_started"
      [("location", location (Procdesc.get_loc proc_desc)); ("call_sites", `List (List.rev calls))]


let call ~caller ~callee ~loc ~route ~reason =
  if Config.pulse_test_observability then
    emit caller "call_route"
      [ ("location", location loc)
      ; ("callee", Option.value_map callee ~default:`Null ~f:(fun p -> `Assoc (proc_fields p)))
      ; ("route", `String route)
      ; ("reason", `String reason) ]


let dispatch_rule ~caller ~callee ~loc ~rule =
  if Config.pulse_test_observability then
    emit caller "dispatch_rule"
      [ ("location", location loc)
      ; ("callee", `Assoc (proc_fields callee))
      ; ("rule_id", `String rule) ]


let complete proc_name ~outcomes ~dropped_disjuncts ~interrupted_loops =
  if Config.pulse_test_observability then
    emit proc_name "analysis_completed"
      [ ("outcomes", `List (List.map outcomes ~f:(fun outcome -> `String outcome)))
      ; ("dropped_disjuncts", `Int dropped_disjuncts)
      ; ("interrupted_loops", `Int interrupted_loops) ]


let incomplete proc_name ~reason = emit proc_name "analysis_incomplete" [("reason", `String reason)]
