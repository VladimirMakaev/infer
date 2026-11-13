(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

(* PulseCItv uses one logging function, [die InternalError], on the unreachable addition of two
   opposite infinities. The oracle turns it into an exception so that a probe reaching it is a
   test failure rather than a silent abort. *)

type kind = InternalError

exception Infer_die of string

let die (_ : kind) fmt = Format.kasprintf (fun s -> raise (Infer_die s)) fmt
