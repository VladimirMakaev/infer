(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

open! IStd
module AbductiveDomain = PulseAbductiveDomain
module AbstractValue = PulseAbstractValue
module CallEvent = PulseCallEvent
module Decompiler = PulseDecompiler
module DecompilerExpr = PulseDecompilerExpr
module ValueHistory = PulseValueHistory
module ValueOrigin = PulseValueOrigin

let add_call_source v call actuals astate =
  AbductiveDomain.map_decompiler astate ~f:(fun decompiler ->
      Decompiler.add_call_source v call actuals decompiler )


let find v {AbductiveDomain.decompiler} = Decompiler.find v decompiler

(* The decompiler holds one expression per abstract value, so expressions that evaluate to the same
   value share an entry and the last one recorded wins. Every zero of a path is one value, so an
   unrelated zero, a container's size among them, can end up holding the name of a null pointer. A
   value loaded from memory still knows which cell it came from, and that origin is kept per
   variable rather than per value, so it survives: name the cell the value was loaded from. *)
let find_from_origin value_origin astate =
  let of_origin (origin : ValueOrigin.t) =
    match origin with
    | InMemory {src= src, _; access; dest= dest, _} ->
        Decompiler.find_access dest access ~src astate.AbductiveDomain.decompiler
    | OnStack _ | Unknown _ ->
        None
  in
  let expr =
    match (value_origin : ValueOrigin.t) with
    | OnStack {var; addr_hist= v, _} ->
        (* reading a temporary rewraps its origin as the temporary's own; the load that bound it
           left the richer origin on the stack *)
        AbductiveDomain.Stack.find_opt var astate
        |> Option.filter ~f:(fun origin -> AbstractValue.equal (ValueOrigin.value origin) v)
        |> Option.bind ~f:of_origin
    | InMemory _ | Unknown _ ->
        of_origin value_origin
  in
  Option.value_or_thunk expr ~default:(fun () -> find (ValueOrigin.value value_origin) astate)


let add_iterator_source v call ~src ~is_reference_receiver astate =
  AbductiveDomain.map_decompiler astate ~f:(fun decompiler ->
      Decompiler.add_iterator_source v call ~src ~is_reference_receiver decompiler )
