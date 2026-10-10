(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

open! IStd

(** Versioned opt-in JSON observations in StatsLogging; no output when disabled. Capture existence
    is independently available from capture.db's procedures.cfg column. *)

val start : Procdesc.t -> unit

val call :
     caller:Procname.t
  -> callee:Procname.t option
  -> loc:Location.t
  -> route:string
  -> reason:string
  -> unit

val dispatch_rule : caller:Procname.t -> callee:Procname.t -> loc:Location.t -> rule:string -> unit

val complete :
  Procname.t -> outcomes:string list -> dropped_disjuncts:int -> interrupted_loops:int -> unit

val incomplete : Procname.t -> reason:string -> unit
