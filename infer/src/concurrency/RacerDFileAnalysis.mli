(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

open! IStd

val build_caller_index : unit -> unit
(** Compute the callee-to-callers table this checker reads, once, before the analysis starts.
    Skipping it is not an error: the checker falls back to scanning the captured procedures. *)

val analyze : RacerDDomain.summary InterproceduralAnalysis.file_t -> IssueLog.t
