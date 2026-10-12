(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

open! IStd

(** Time long-running operations listed in {!Timeable} under a timeout *)

type state

exception Timeout of float

(** raised when the budget of the enclosing {!with_total_budget} is exhausted; unlike {!Timeout} it
    is not caught by {!time}, so it unwinds the whole unit of work instead of one checker *)
exception TotalTimeout of float

val check_timeout : unit -> unit
(** check if the timer has been running for more than {!Config.timeout} seconds, and if the
    enclosing {!with_total_budget} has been running for more than {!Config.timeout_total} seconds *)

val time : Timeable.t -> on_timeout:(float -> 'a) -> f:(unit -> 'a) -> 'a

val with_total_budget : on_timeout:(float -> 'a) -> f:(unit -> 'a) -> 'a
(** run [f] under a {!Config.timeout_total} budget which, unlike the one of {!time}, the nested
    calls to {!time} underneath it do not restart. A no-op when {!Config.timeout_total} is unset. *)

val suspend : unit -> state

val resume : state -> unit
