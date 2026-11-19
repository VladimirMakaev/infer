(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

(** Module to manage the table of attributes. *)

open! IStd

val store :
     ?check_stored_definition:(ProcAttributes.t -> unit)
  -> proc_desc:Procdesc.t option
  -> ProcAttributes.t
  -> analysis:bool
  -> unit
(** Save .attr file for the procedure into the attributes database. [check_stored_definition]
    receives the attributes of the procedure already stored with a CFG under the same name. *)

val iter_procs_and_callees :
  ?filter_callees:(Procname.t list -> bool) -> (Procname.t -> Procname.t list -> unit) -> unit
(** Run a function on every procedure that has a CFG, passing its syntactic callees. Reads the
    procedure tables of both databases, analysis first, so it is usable during the analysis; it
    analyses nothing. {!SyntacticCallGraph.iter_captured_procs_and_callees} is the pre-analysis
    equivalent and reads the capture database only.

    [filter_callees] is applied to a row's callees before its attributes are deserialised, which is
    by far the dearer of the two. A caller interested in a few callees should pass it rather than
    filter inside the function, especially when the scan is repeated. *)

val load : Procname.t -> ProcAttributes.t option
(** Load the attributes for the procedure from the attributes database. *)

val load_exn : Procname.t -> ProcAttributes.t
(** like [load], but raises an exception if no attributes are found. *)

val is_no_return : Procname.t -> bool

val load_formal_types : Procname.t -> Typ.t list
(** Returns all the formal types of a given procedure. *)

val clear_cache : unit -> unit
(** clear attribute cache *)

val set_lru_limit : lru_limit:int option -> unit
