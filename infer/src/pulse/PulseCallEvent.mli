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
  | Call of Procname.t  (** known function with summary *)
  | Model of string  (** hardcoded model with hardcoded name *)
  | ModelName of Procname.t  (** hardcoded model taking the name of the original procedure *)
  | SkippedKnownCall of Procname.t * skipped_call_cause  (** known function treated as unknown *)
  | SkippedUnknownCall of Exp.t  (** couldn't link the expression to a proc name *)
[@@deriving compare, equal, yojson_of]

val pp : F.formatter -> t -> unit

val describe : F.formatter -> t -> unit

val pp_for_report : F.formatter -> t -> unit
(** Legacy report text with verbose callee names, without diagnostic skipped-call causes. *)

val pp_name_only : with_class:bool -> F.formatter -> t -> unit

val to_name_only : t -> string
