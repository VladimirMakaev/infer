(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

(* Stand-in for Infer's IStd, which re-exports Core (plus Die).
   PulseCItv.ml uses only Option (with Core's labelled signatures) and the Die error
   constructors, so this stub supplies exactly those and nothing else. *)

(* Infer's IStd re-exports Core, which is where the deriving plugins find compare_int,
   equal_int, yojson_of_int and friends; the stub re-exports the same builtins. *)
include Ppx_compare_lib.Builtin
include Ppx_yojson_conv_lib.Yojson_conv.Primitives

type error = InternalError | UserError | ExternalError

module Option = struct
  type 'a t = 'a option = None | Some of 'a

  let some x = Some x

  let is_none = function None -> true | Some _ -> false

  let is_some = function None -> false | Some _ -> true

  let bind o ~f = match o with None -> None | Some x -> f x

  let map o ~f = match o with None -> None | Some x -> Some (f x)

  let value o ~default = match o with None -> default | Some x -> x

  let first_some o1 o2 = match o1 with Some _ -> o1 | None -> o2

  module Monad_infix = struct
    let ( >>= ) o f = bind o ~f

    let ( >>| ) o f = map o ~f
  end
end
