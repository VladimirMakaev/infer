(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

open! IStd

(** [callee -> the procedures that call it], over the call edges recorded at capture.

    The table is computed once, before the analysis, and read with an indexed lookup during it, so
    that a checker asking which procedures call a given one does not scan every captured procedure
    to find out. It holds only the edges whose callee passes the [keep_callee] given to {!build}:
    that filter is part of the table's contract, and a reader wanting other edges has to widen the
    build rather than only the query.

    Precomputing is sound because the procedures tables are written at capture and not during the
    analysis -- the one write that targets the analysis database, [Procdesc.mark_if_unchanged], runs
    from [SourceFiles.add] on the capture path -- so the table cannot go stale under a reader. *)

(** The construction rules, separated from the database so they can be tested on their own: a
    self-call is never an edge, a caller appears once per callee however many times it calls it, and
    the rows come out in a fixed order. *)
module Edges : sig
  type t

  val empty : t

  val add :
    t -> keep_callee:(Procname.t -> bool) -> caller_uid:string -> callees:Procname.t list -> t

  val rows : t -> (string * string list) list
  (** [(callee unique id, caller unique ids)], each caller once, sorted within a row *)

  val count_edges : t -> int
end

val build : keep_callee:(Procname.t -> bool) -> unit -> unit
(** One pass over the recorded call edges, writing the table. Call it before the analysis starts,
    from the process that owns the results directory. *)

val callers_of : Procname.Set.t -> Procname.t list Procname.Map.t option
(** The callers of each of [callees] that has any, or [None] if {!build} did not run -- RacerD off,
    or the switch set -- in which case the caller must answer the question the slow way rather than
    take the silence for an answer. A results directory whose schema predates this table does not
    reach here: it is refused by the run-state check, since the table is in [Database.schema_hum].
*)
