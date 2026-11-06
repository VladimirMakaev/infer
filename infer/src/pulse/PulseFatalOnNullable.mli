(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

open! IStd
open PulseDomainInterface

val report_errors : _ InterproceduralAnalysis.t -> (ExecutionDomain.t * PathContext.t) list -> unit
(** Report [PULSE_FATAL_ON_NULLABLE_RETURN] for each call whose documented null return is what ends
    the program. Takes the procedure's posts BEFORE they are summarised: summarising discards the
    attribute the rule is about.

    Does nothing unless that issue type is enabled. The rule is unmeasured, and the test suites run
    with [--no-filtering], so filtering at report time would not keep it out of their expectations.
*)
