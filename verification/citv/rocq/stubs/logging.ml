(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

(* Stand-in for Infer's Logging: PulseCItv.ml uses only [L.die InternalError fmt ...]. *)

let die (_ : IStd.error) fmt = Format.kasprintf failwith fmt
