(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

open! IStd
module F = Format
module L = Logging

(* a humble debug mechanism: set [debug] to [true] and run [make -C infer/src runtest] to see the
    arithmetic engine at work in more details *)
(* change this to [true] for more debug information *)
let debug = false

let dummy_formatter = F.make_formatter (fun _ _ _ -> ()) (fun () -> ())

let p fmt =
  if debug then if Config.is_running_unit_test then F.printf fmt else L.d_printf fmt
  else F.ifprintf dummy_formatter fmt


(* ── JSONL trace sink for formula oracle generation ── *)

let trace_channel : Out_channel.t option ref = ref None

let tracing_active () = Option.is_some !trace_channel

let set_trace_file path =
  trace_channel := Some (Out_channel.create path)

let close_trace () =
  Option.iter !trace_channel ~f:Out_channel.close ;
  trace_channel := None

let trace_json (json : Yojson.Safe.t) =
  match !trace_channel with
  | None ->
      ()
  | Some ch ->
      Yojson.Safe.to_channel ch json ;
      Out_channel.output_char ch '\n' ;
      Out_channel.flush ch
