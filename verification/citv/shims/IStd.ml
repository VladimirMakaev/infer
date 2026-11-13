(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

(* PulseCItv opens IStd and uses five Option functions from it plus the monadic operators.
   They are reproduced here with the same meanings, so that the module under test needs no
   edit and the oracle needs no dependency beyond zarith and yojson. *)

module Option = struct
  include Stdlib.Option

  let value o ~default = match o with Some x -> x | None -> default

  let bind o ~f = match o with Some x -> f x | None -> None

  let map o ~f = match o with Some x -> Some (f x) | None -> None

  let first_some a b = match a with Some _ -> a | None -> b

  let is_none = function None -> true | Some _ -> false

  let some x = Some x

  module Monad_infix = struct
    let ( >>= ) o f = bind o ~f

    let ( >>| ) o f = map o ~f
  end
end
