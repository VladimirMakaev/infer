(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

open! IStd
open PulseBasicInterface
open PulseDomainInterface

val pinned_null : AbductiveDomain.t -> Attribute.FailureOriginSet.t
(** the failure origins this state forces to be null, capped the way the attribute itself is *)

val not_pinned_null : AbductiveDomain.t -> Attribute.FailureOriginSet.t
(** the failure origins this state mentions and does NOT force to be null. Not the complement of
    {!pinned_null}: an origin the state never mentions is in neither. *)
