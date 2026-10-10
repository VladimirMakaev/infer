(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

open! IStd
module F = Format
module IRAttributes = Attributes
open PulseBasicInterface
module AbductiveDomain = PulseAbductiveDomain
module BaseMemory = PulseBaseMemory
module BaseStack = PulseBaseStack
module DecompilerExpr = PulseDecompilerExpr
module ExecutionDomain = PulseExecutionDomain
module PathContext = PulsePathContext

(** Unnecessary copies are tracked in two places:

    - In non-disjunctive domain (here), we keep track of a map from (copy var, source address
      (optional)) -> Modified/ Copied heap snapshot
    - In attributes ({!PulseBaseAddressAttributes}) of each disjunct where we keep track of
    - source address -> copy var ({!CopiedVar})
    - copy address -> source address ({!SourceOriginOfCopy})

    In order to determine if a source/copy variable is modified,

    - When a source variable goes out of scope, we first lookup the CopiedVar from the attributes of
      source address and then lookup the snapshot from the non-disjunctive domain
    - When a copy variable goes out of scope, we first lookup the corresponding source address which
      we can then use to loopkup the snapshot from the non-disjunctive domain

    Then we compare the snapshot heap with the current heap (see {!PulseNonDisjunctiveOperations}.)
*)
open AbstractDomain.Types

module MakeDomainFromTotalOrder (M : AbstractDomain.Comparable) = struct
  type t = M.t

  let leq ~lhs ~rhs = M.leq ~lhs ~rhs

  (* NOTE: This [join] definition is incorrect when given arguments do not have a total order. Make sure
     that the function is always applied to the arguments that have a total order. *)
  let join x y = if leq ~lhs:x ~rhs:y then y else x

  let widen ~prev ~next ~num_iters:_ = join prev next

  let pp fmt = M.pp fmt
end

type copy_spec_t =
  | Copied of
      { source_typ: Typ.t option
      ; source_opt: DecompilerExpr.source_expr option
      ; node: Procdesc.Node.t
      ; location: Location.t
      ; copied_location: (Procname.t * Location.t) option
      ; heap: BaseMemory.t
      ; from: Attribute.CopyOrigin.t
      ; timestamp: Timestamp.t }
  | Modified of
      { source_typ: Typ.t option
      ; source_opt: DecompilerExpr.source_expr option
      ; node: Procdesc.Node.t
      ; location: Location.t
      ; copied_location: (Procname.t * Location.t) option
      ; from: Attribute.CopyOrigin.t
      ; copied_timestamp: Timestamp.t }
[@@deriving equal]

module CopySpec = MakeDomainFromTotalOrder (struct
  type t = copy_spec_t [@@deriving equal]

  let leq ~lhs ~rhs =
    match (lhs, rhs) with
    | Copied _, _ ->
        true
    | Modified _, Copied _ ->
        false
    | Modified _, Modified _ ->
        true


  let pp fmt = function
    | Copied {source_typ; heap; location; from; timestamp} ->
        Format.fprintf fmt "@[%a (value of type %a) at %a@ with heap= %a@ (timestamp: %d)@]"
          Attribute.CopyOrigin.pp from
          (Pp.option (Typ.pp Pp.text))
          source_typ Location.pp location BaseMemory.pp heap
          (timestamp :> int)
    | Modified _ ->
        Format.fprintf fmt "modified"
end)

module CopyVar = struct
  type t = {copied_into: Attribute.CopiedInto.t; source_addr_opt: AbstractValue.t option}
  [@@deriving compare]

  let pp fmt {copied_into; source_addr_opt} =
    match source_addr_opt with
    | Some source_addr ->
        Format.fprintf fmt "%a copied with source_addr %a" Attribute.CopiedInto.pp copied_into
          AbstractValue.pp source_addr
    | None ->
        Format.fprintf fmt "%a copied" Attribute.CopiedInto.pp copied_into
end

type parameter_spec_t =
  | Unmodified of {typ: Typ.t; location: Location.t; heap: BaseMemory.t}
  | Modified
[@@deriving equal]

module ParameterSpec = MakeDomainFromTotalOrder (struct
  type t = parameter_spec_t [@@deriving equal]

  let leq ~lhs ~rhs =
    match (lhs, rhs) with
    | Unmodified _, _ ->
        true
    | Modified, Unmodified _ ->
        false
    | Modified, Modified ->
        true


  let pp fmt = function
    | Unmodified {typ; heap; location} ->
        Format.fprintf fmt "@[const refable (value of type %a) at %a@ with heap= %a@]"
          (Typ.pp Pp.text) typ Location.pp location BaseMemory.pp heap
    | Modified ->
        Format.fprintf fmt "modified"
end)

module ParameterVar = struct
  type t = Var.t [@@deriving compare]

  let pp fmt = Var.pp fmt
end

module DestructorChecked = AbstractDomain.FiniteSet (Var)

module Captured = struct
  include
    AbstractDomain.FiniteMultiMap
      (struct
        include Pvar

        let pp = Pvar.pp Pp.text
      end)
      (struct
        type t = CapturedVar.capture_mode [@@deriving compare]

        let pp f x = F.pp_print_string f (CapturedVar.string_of_capture_mode x)
      end)

  let mem_var var x =
    match (var : Var.t) with ProgramVar pvar -> mem pvar x | LogicalVar _ -> false


  let is_captured_by_ref var x =
    match (var : Var.t) with
    | ProgramVar pvar ->
        find_all pvar x
        |> List.exists ~f:(fun mode -> CapturedVar.equal_capture_mode mode ByReference)
    | LogicalVar _ ->
        false
end

module CopyMap = struct
  include AbstractDomain.Map (CopyVar) (CopySpec)

  let remove_var var x =
    filter
      (fun {copied_into} _ ->
        match copied_into with
        | IntoVar {copied_var} | IntoIntermediate {copied_var} ->
            not (Var.equal var copied_var)
        | IntoField _ ->
            true )
      x
end

module ParameterMap = AbstractDomain.Map (ParameterVar) (ParameterSpec)

(** the copies into variables that reached the end of their scope on a path where they were made *)
module ReachedEnd = AbstractDomain.FiniteSet (CopyVar)

(** the intermediates whose source is used by another argument of the call they are passed to *)
module SharedIntermediates = AbstractDomain.FiniteSet (Var)

module Locked = AbstractDomain.BooleanOr
module TrackedLoc = AbstractDomain.FiniteMultiMap (Location) (Timestamp)

module CapturedSource = struct
  type t = {source: AbstractValue.t; location: Location.t; timestamp: Timestamp.t}
  [@@deriving compare]

  let pp fmt {source; location; timestamp} =
    F.fprintf fmt "%a at %a (timestamp: %d)" AbstractValue.pp source Location.pp location
      (timestamp :> int)
end

module CapturedSources = AbstractDomain.FiniteSet (CapturedSource)
module ClosureCaptures = AbstractDomain.FiniteMultiMap (AbstractValue) (CapturedSource)
module EscapedCopies = AbstractDomain.FiniteSet (CopyVar)

(** The value domain [Val] is conceptually a collection, i.e. a set or a map, that has a bottom
    value and an element can be added to it, e.g. using [f_add_v] in the [add] function. *)
module MakeMapToCollection
    (Key : PrettyPrintable.PrintableOrderedType)
    (Val : AbstractDomain.WithBottom) =
struct
  include AbstractDomain.Map (Key) (Val)

  let value_with_bottom = Option.value ~default:Val.bottom

  let find k x = find_opt k x |> value_with_bottom

  let add k f_add_v x = update k (fun v_opt -> Some (f_add_v (value_with_bottom v_opt))) x
end

module MakeSetWithTrackedLoc (Key : PrettyPrintable.PrintableOrderedType) = struct
  include MakeMapToCollection (Key) (TrackedLoc)

  let add k loc timestamp map = add k (TrackedLoc.add loc timestamp) map
end

module Loads = struct
  module IdentToVars = AbstractDomain.FiniteMultiMap (Ident) (Var)
  module LoadedVars = MakeSetWithTrackedLoc (Var)
  include AbstractDomain.PairWithBottom (IdentToVars) (LoadedVars)

  let add loc timestamp ident var (ident_to_vars, loaded_vars) =
    (IdentToVars.add ident var ident_to_vars, LoadedVars.add var loc timestamp loaded_vars)


  let get_all ident (ident_to_vars, _) = IdentToVars.find_all ident ident_to_vars

  let add_use loc timestamp var (ident_to_vars, loaded_vars) =
    (ident_to_vars, LoadedVars.add var loc timestamp loaded_vars)


  let is_loaded var (_, loaded_vars) = LoadedVars.mem var loaded_vars

  let get_loaded_locations var (_, loaded_vars) = LoadedVars.find var loaded_vars
end

module PVar = struct
  type t = Pvar.t [@@deriving compare]

  let pp = Pvar.pp Pp.text
end

module Stores = MakeSetWithTrackedLoc (PVar)

module CalleeWithUnknown = struct
  type t = V of {copy_tgt: Exp.t option; callee: Procname.t} | Unknown [@@deriving compare]

  let pp_copy_tgt f copy_tgt = Option.iter copy_tgt ~f:(fun tgt -> F.fprintf f "(%a)" Exp.pp tgt)

  let pp f = function
    | V {copy_tgt; callee} ->
        F.fprintf f "%a%a" Procname.pp callee pp_copy_tgt copy_tgt
    | Unknown ->
        F.pp_print_string f "unknown"


  let is_copy_to_field_or_global = function
    | V {copy_tgt= Some (Lindex _ | Lfield _); callee} ->
        Option.exists (IRAttributes.load callee) ~f:(fun attrs ->
            attrs.ProcAttributes.is_cpp_copy_assignment || attrs.ProcAttributes.is_cpp_copy_ctor )
    | V {copy_tgt= Some (Lvar pvar); callee} ->
        Pvar.is_global pvar
        && Option.exists (IRAttributes.load callee) ~f:(fun attrs ->
            attrs.ProcAttributes.is_cpp_copy_assignment )
    | V _ ->
        false
    | Unknown ->
        true


  let is_moved = function
    | V {callee} ->
        Option.exists (IRAttributes.load callee) ~f:(fun attrs ->
            attrs.ProcAttributes.is_cpp_move_ctor )
    | Unknown ->
        true
end

module CalleesWithLoc = struct
  include MakeSetWithTrackedLoc (CalleeWithUnknown)

  let is_copy_to_field_or_global x =
    exists (fun callee _ -> CalleeWithUnknown.is_copy_to_field_or_global callee) x


  let is_moved x = exists (fun callee _ -> CalleeWithUnknown.is_moved callee) x
end

module PassedTo = struct
  include MakeMapToCollection (Var) (CalleesWithLoc)

  let add var callee loc timestamp x = add var (CalleesWithLoc.add callee loc timestamp) x

  let is_copied_to_field_or_global var x =
    find_opt var x |> Option.exists ~f:CalleesWithLoc.is_copy_to_field_or_global


  let is_moved var x = find_opt var x |> Option.exists ~f:CalleesWithLoc.is_moved
end

module IntraDomElt = struct
  type t =
    { copy_map: CopyMap.t
    ; parameter_map: ParameterMap.t
    ; destructor_checked: DestructorChecked.t
    ; captured: Captured.t
    ; closure_captures: ClosureCaptures.t
    ; escaped_sources: CapturedSources.t
    ; escaped_copies: EscapedCopies.t
    ; locked: Locked.t
    ; loads: Loads.t
    ; stores: Stores.t
    ; passed_to: PassedTo.t
    ; reached_end: ReachedEnd.t
    ; shared_intermediates: SharedIntermediates.t }
  [@@deriving abstract_domain]

  let pp fmt
      { copy_map
      ; parameter_map
      ; destructor_checked
      ; captured
      ; closure_captures
      ; escaped_sources
      ; escaped_copies
      ; locked
      ; loads
      ; passed_to
      ; reached_end
      ; shared_intermediates } =
    F.fprintf fmt
      "@[@[copy map: %a@],@ @[parameter map: %a@],@ @[destructor checked: %a@],@ @[captured: \
       %a@],@ @[locked: %a@],@ @[loads: %a@],@ @[passed to: %a@],@ @[reached end: %a@],@ @[shared \
       intermediates: %a@],@ @[closure captures: %a@],@ @[escaped sources: %a@],@ @[escaped \
       copies: %a@]@]"
      CopyMap.pp copy_map ParameterMap.pp parameter_map DestructorChecked.pp destructor_checked
      Captured.pp captured Locked.pp locked Loads.pp loads PassedTo.pp passed_to ReachedEnd.pp
      reached_end SharedIntermediates.pp shared_intermediates ClosureCaptures.pp closure_captures
      CapturedSources.pp escaped_sources EscapedCopies.pp escaped_copies


  let bottom =
    { copy_map= CopyMap.empty
    ; parameter_map= ParameterMap.empty
    ; destructor_checked= DestructorChecked.empty
    ; captured= Captured.bottom
    ; closure_captures= ClosureCaptures.bottom
    ; escaped_sources= CapturedSources.empty
    ; escaped_copies= EscapedCopies.empty
    ; locked= Locked.bottom
    ; loads= Loads.bottom
    ; stores= Stores.bottom
    ; passed_to= PassedTo.bottom
    ; reached_end= ReachedEnd.empty
    ; shared_intermediates= SharedIntermediates.empty }


  let is_bottom
      { copy_map
      ; parameter_map
      ; destructor_checked
      ; captured
      ; closure_captures
      ; escaped_sources
      ; escaped_copies
      ; locked
      ; loads
      ; stores
      ; passed_to
      ; reached_end
      ; shared_intermediates } =
    CopyMap.is_bottom copy_map && ReachedEnd.is_bottom reached_end
    && SharedIntermediates.is_bottom shared_intermediates
    && ParameterMap.is_bottom parameter_map
    && DestructorChecked.is_bottom destructor_checked
    && Captured.is_bottom captured && Locked.is_bottom locked && Loads.is_bottom loads
    && Stores.is_bottom stores && PassedTo.is_bottom passed_to
    && ClosureCaptures.is_bottom closure_captures
    && CapturedSources.is_bottom escaped_sources
    && EscapedCopies.is_bottom escaped_copies


  let mark_copy_as_modified ?(reached_end = false) ~is_modified ~copied_into ~source_addr_opt
      ({copy_map} as astate_n) =
    let copy_var = CopyVar.{copied_into; source_addr_opt} in
    let astate_n =
      if reached_end then {astate_n with reached_end= ReachedEnd.add copy_var astate_n.reached_end}
      else astate_n
    in
    let copy_map =
      match CopyMap.find_opt copy_var copy_map with
      | Some
          (Copied
             { source_typ
             ; source_opt
             ; from
             ; node
             ; copied_location
             ; location
             ; heap= copy_heap
             ; timestamp= copied_timestamp } )
        when is_modified copy_heap copied_timestamp ->
          Logging.d_printfln_escaped "Copy/source modified!" ;
          let modified : copy_spec_t =
            Modified
              {source_typ; source_opt; node; location; copied_location; from; copied_timestamp}
          in
          CopyMap.add copy_var modified copy_map
      | _ ->
          copy_map
    in
    {astate_n with copy_map}


  let mark_copies_into_var_as_modified var ({copy_map} as astate_n) =
    let copy_map =
      CopyMap.mapi
        (fun CopyVar.{copied_into} (copy_spec : CopySpec.t) : CopySpec.t ->
          match (copied_into, copy_spec) with
          | ( IntoVar {copied_var}
            , Copied
                { source_typ
                ; source_opt
                ; from
                ; node
                ; copied_location
                ; location
                ; timestamp= copied_timestamp } )
            when Var.equal var copied_var ->
              Modified
                {source_typ; source_opt; node; location; copied_location; from; copied_timestamp}
          | _ ->
              copy_spec )
        copy_map
    in
    {astate_n with copy_map}


  let mark_parameter_as_modified ~is_modified ~var ({parameter_map} as astate_n) =
    let parameter_map =
      match ParameterMap.find_opt var parameter_map with
      | Some (Unmodified {heap= copy_heap}) when is_modified copy_heap Timestamp.t0 ->
          Logging.d_printfln_escaped "Parameter %a modified!" Var.pp var ;
          ParameterMap.add var Modified parameter_map
      | _ ->
          parameter_map
    in
    {astate_n with parameter_map}


  let checked_via_destructor var astate_n =
    {astate_n with destructor_checked= DestructorChecked.add var astate_n.destructor_checked}


  let is_never_used_after_copy_into_intermediate_or_field copy_var pvar
      (copied_location : Location.t) (copied_timestamp : Timestamp.t)
      {passed_to; loads; stores; escaped_copies} =
    let is_after_copy =
      TrackedLoc.exists (fun location timestamp ->
          SourceFile.equal copied_location.file location.file
          && copied_location.line <= location.line
          && (copied_timestamp :> int) < (timestamp :> int) )
    in
    let source_var = Var.of_pvar pvar in
    let is_passed_to_non_destructor_after_copy =
      PassedTo.find source_var passed_to
      |> CalleesWithLoc.exists (fun callee tracked_loc ->
          match callee with
          | V {callee} ->
              is_after_copy tracked_loc && not (Procname.is_destructor callee)
          | Unknown ->
              false )
    in
    let is_loaded_after_copy = Loads.get_loaded_locations source_var loads |> is_after_copy in
    let is_stored_after_copy = Stores.find pvar stores |> is_after_copy in
    not
      ( EscapedCopies.mem copy_var escaped_copies
      || is_loaded_after_copy || is_stored_after_copy || is_passed_to_non_destructor_after_copy )


  module CopiedSet = PrettyPrintable.MakePPSet (Attribute.CopiedInto)

  let is_lvalue_ref_param ~ref_formals pvar =
    Option.exists (List.Assoc.find ref_formals ~equal:Pvar.equal pvar) ~f:(fun typ ->
        not (Typ.is_rvalue_reference typ) )


  let get_copied ~ref_formals ~ptr_formals
      ({copy_map; captured; reached_end; shared_intermediates} as astate_n) =
    let modified =
      CopyMap.fold
        (fun CopyVar.{copied_into} (copy_spec : CopySpec.t) acc ->
          match copy_spec with Modified _ -> CopiedSet.add copied_into acc | Copied _ -> acc )
        copy_map CopiedSet.empty
    in
    let is_captured copy_into =
      match (copy_into : Attribute.CopiedInto.t) with
      | IntoVar {copied_var= ProgramVar pvar} | IntoIntermediate {copied_var= ProgramVar pvar} ->
          Captured.mem pvar captured
      | _ ->
          false
    in
    CopyMap.fold
      (fun (CopyVar.{copied_into} as copy_var) (copy_spec : CopySpec.t) acc ->
        match (copied_into, copy_spec) with
        | _, _ when is_captured copied_into ->
            acc
        | IntoIntermediate {copied_var}, _
          when SharedIntermediates.mem copied_var shared_intermediates ->
            acc
        | (IntoVar _ | IntoIntermediate _), Copied _ when not (ReachedEnd.mem copy_var reached_end)
          ->
            (* the copy is checked at the end of the scope of its variable, which no path where the
               copy was made reached, e.g. because their disjuncts were dropped *)
            acc
        | _, Copied _ when CopiedSet.mem copied_into modified ->
            acc
        | ( (IntoField _ | IntoIntermediate _)
          , ( Copied
                { node
                ; location
                ; copied_location
                ; source_typ
                ; source_opt= Some (PVar pvar, _) as source_opt
                ; from
                ; timestamp= copied_timestamp }
            | Modified
                { node
                ; location
                ; copied_location
                ; source_typ
                ; source_opt= Some (PVar pvar, _) as source_opt
                ; from
                ; copied_timestamp } ) ) ->
            if
              (not (Pvar.is_global pvar))
              && (not (is_lvalue_ref_param ~ref_formals pvar))
              && (not (List.Assoc.mem ptr_formals ~equal:Pvar.equal pvar))
              &&
              (* Note: We should NOT use [copied_location] here, because which is highly liked a
                    location of another procedure, so is unhelpful to reason about intra-procedrual
                    order. *)
              is_never_used_after_copy_into_intermediate_or_field copy_var pvar location
                copied_timestamp astate_n
            then
              (* if source var is never used later on, we can still suggest removing the copy even though the copy is modified *)
              (copied_into, source_typ, source_opt, node, location, copied_location, from) :: acc
            else acc
        | _, Copied {node; location; copied_location; source_typ; source_opt; from} ->
            (copied_into, source_typ, source_opt, node, location, copied_location, from) :: acc
        | _, Modified _ ->
            acc )
      copy_map []


  let get_const_refable_parameters {parameter_map; captured; loads; passed_to} =
    ParameterMap.fold
      (fun var (parameter_spec_t : ParameterSpec.t) acc ->
        if Captured.is_captured_by_ref var captured then acc
        else if
          (Loads.is_loaded var loads || Captured.mem_var var captured)
          && (not (PassedTo.is_copied_to_field_or_global var passed_to))
          && not (PassedTo.is_moved var passed_to)
        then
          match parameter_spec_t with
          | Modified ->
              acc
          | Unmodified {location; typ= copied_source_typ} ->
              (var, copied_source_typ, location) :: acc
        else acc )
      parameter_map []


  let add_var copied_into ~source_addr_opt res astate_n =
    {astate_n with copy_map= CopyMap.add {copied_into; source_addr_opt} res astate_n.copy_map}


  let remove_var var astate_n = {astate_n with copy_map= CopyMap.remove_var var astate_n.copy_map}

  let has_intermediate_copies {copy_map} =
    CopyMap.exists
      (fun CopyVar.{copied_into; source_addr_opt} _ ->
        match (copied_into, source_addr_opt) with IntoIntermediate _, Some _ -> true | _ -> false )
      copy_map


  (* The order of evaluation of the arguments of a call is unspecified, so a variable that is copied
     into an intermediate for one argument and also used by another argument cannot be moved. *)
  let mark_intermediates_with_shared_source actuals astate ({copy_map} as astate_n) =
    let get_repr = Formula.get_var_repr astate.AbductiveDomain.path_condition in
    let equal_address left right = AbstractValue.equal (get_repr left) (get_repr right) in
    let intermediates_of (actual, actual_addr_opt) =
      CopyMap.fold
        (fun CopyVar.{copied_into; source_addr_opt} _ intermediates ->
          match (copied_into, source_addr_opt) with
          | IntoIntermediate {copied_var}, Some source ->
              let is_actual =
                match (actual : Exp.t) with
                | Lvar pvar when Var.equal copied_var (Var.of_pvar pvar) ->
                    true
                | _ ->
                    Option.exists actual_addr_opt ~f:(fun actual_addr ->
                        BaseStack.find_opt copied_var
                          (astate.AbductiveDomain.post :> PulseBaseDomain.t).stack
                        |> Option.exists ~f:(fun target ->
                            equal_address actual_addr (ValueOrigin.value target) ) )
              in
              if is_actual then (copied_var, source) :: intermediates else intermediates
          | _ ->
              intermediates )
        copy_map []
    in
    let actuals =
      List.mapi actuals ~f:(fun index ((_, address) as actual) ->
          (index, address, intermediates_of actual) )
    in
    List.fold actuals ~init:astate_n ~f:(fun astate_n (index, _, intermediates) ->
        List.fold intermediates ~init:astate_n ~f:(fun astate_n (copied_var, source) ->
            let shared =
              List.exists actuals ~f:(fun (other_index, address, copies) ->
                  (not (Int.equal index other_index))
                  && ( Option.exists address ~f:(equal_address source)
                     || List.exists copies ~f:(fun (_, other_source) ->
                         equal_address source other_source ) ) )
            in
            if shared then
              { astate_n with
                shared_intermediates=
                  SharedIntermediates.add copied_var astate_n.shared_intermediates }
            else astate_n ) )


  let add_field copied_field ~source_addr_opt res astate_n =
    { astate_n with
      copy_map=
        CopyMap.add
          {copied_into= IntoField {field= copied_field}; source_addr_opt}
          res astate_n.copy_map }


  let add_parameter parameter_var res astate_n =
    {astate_n with parameter_map= ParameterMap.add parameter_var res astate_n.parameter_map}


  let is_checked_via_destructor var {destructor_checked} =
    DestructorChecked.mem var destructor_checked


  let set_captured_variables exp astate_n =
    match exp with
    | Exp.Closure {captured_vars} ->
        List.fold captured_vars ~init:astate_n
          ~f:(fun astate_n (captured_exp, {CapturedVar.pvar; capture_mode}) ->
            let captured = Captured.add pvar capture_mode astate_n.captured in
            let captured =
              match (captured_exp : Exp.t) with
              | Lvar tmp when Pvar.is_frontend_tmp tmp ->
                  (* the clang frontend copies arrays captured by value into a temporary and
                     captures its address *)
                  Captured.add tmp capture_mode captured
              | _ ->
                  captured
            in
            {astate_n with captured} )
    | _ ->
        astate_n


  (* Address identity follows closure copies and reference/pointer aliases in the heap, without
     retaining an old binding when a pointer is reassigned. Do not follow reference captures here:
     they are the sources of uses, not additional closures passed as arguments. *)
  let captured_sources_of_addresses ?(filter = fun _ -> true) addresses astate {closure_captures} =
    if ClosureCaptures.is_bottom closure_captures then CapturedSources.empty
    else
      let reachable =
        AbductiveDomain.reachable_addresses_from (Stdlib.List.to_seq addresses)
          ~edge_filter:(function
            | Access.FieldAccess field ->
                not (Fieldname.is_capture_field_in_closure_by_ref field)
            | Dereference | ArrayAccess _ ->
                true )
          astate `Post
      in
      let get_repr = Formula.get_var_repr astate.AbductiveDomain.path_condition in
      ClosureCaptures.fold
        (fun closure captured sources ->
          if AbstractValue.Set.mem (get_repr closure) reachable && filter closure then
            CapturedSources.add captured sources
          else sources )
        closure_captures CapturedSources.empty


  let record_capture_uses loc (timestamp : Timestamp.t) ~escapes sources astate
      ({copy_map; loads; escaped_copies} as astate_n) =
    if CapturedSources.is_bottom sources then astate_n
    else
      let roots =
        CapturedSources.fold
          (fun {CapturedSource.source; timestamp= captured_timestamp} roots ->
            if (captured_timestamp :> int) <= (timestamp :> int) then source :: roots else roots )
          sources []
      in
      let reachable =
        AbductiveDomain.reachable_addresses_from (Stdlib.List.to_seq roots) astate `Post
      in
      let get_repr = Formula.get_var_repr astate.AbductiveDomain.path_condition in
      let loads, escaped_copies =
        CopyMap.fold
          (fun ({CopyVar.source_addr_opt} as copy_var) (spec : CopySpec.t) (loads, escaped_copies)
             ->
            match (source_addr_opt, spec) with
            | ( Some source
              , ( Copied {source_opt= Some (PVar pvar, _)}
                | Modified {source_opt= Some (PVar pvar, _)} ) )
              when AbstractValue.Set.mem (get_repr source) reachable ->
                ( Loads.add_use loc timestamp (Var.of_pvar pvar) loads
                , if escapes then EscapedCopies.add copy_var escaped_copies else escaped_copies )
            | _ ->
                (loads, escaped_copies) )
          copy_map (loads, escaped_copies)
      in
      {astate_n with loads; escaped_copies}


  (* An opaque callback consumer may retain a pointee it sees now, even if a captured pointer is
     later reassigned. Preserve the reachable addresses at the escape as well as the capture. *)
  let freeze_escaped_sources loc timestamp sources astate =
    CapturedSources.fold
      (fun {CapturedSource.source} frozen ->
        let reachable = AbductiveDomain.reachable_addresses_from (Seq.return source) astate `Post in
        AbstractValue.Set.fold
          (fun source frozen ->
            CapturedSources.add {CapturedSource.source; location= loc; timestamp} frozen )
          reachable frozen )
      sources CapturedSources.empty


  let record_closure_store loc timestamp ~rhs_addr ~is_escape astate astate_n =
    let is_cpp_closure =
      Option.exists (Formula.get_dynamic_type rhs_addr astate.AbductiveDomain.path_condition)
        ~f:(fun {Formula.typ} ->
          match typ.Typ.desc with Tstruct (CppClass _) -> true | _ -> false )
    in
    let astate_n =
      if is_cpp_closure && not (ClosureCaptures.mem rhs_addr astate_n.closure_captures) then
        let closure_captures =
          AbductiveDomain.Memory.fold_edges rhs_addr astate ~init:astate_n.closure_captures
            ~f:(fun captures (access, (source, _)) ->
              match access with
              | Access.FieldAccess field when Fieldname.is_capture_field_in_closure_by_ref field ->
                  AbductiveDomain.Memory.find_edge_opt source Dereference astate
                  |> Option.value_map ~default:captures ~f:(fun (source, _) ->
                      ClosureCaptures.add rhs_addr
                        {CapturedSource.source; location= loc; timestamp}
                        captures )
              | _ ->
                  captures )
        in
        {astate_n with closure_captures}
      else astate_n
    in
    let sources = captured_sources_of_addresses [rhs_addr] astate astate_n in
    if CapturedSources.is_bottom sources || not (is_escape ()) then astate_n
    else
      let sources = freeze_escaped_sources loc timestamp sources astate in
      let escaped_sources = CapturedSources.join astate_n.escaped_sources sources in
      record_capture_uses loc timestamp ~escapes:true escaped_sources astate
        {astate_n with escaped_sources}


  let record_closure_load ~src ~dst astate astate_n =
    let sources = captured_sources_of_addresses [src] astate astate_n in
    if CapturedSources.is_bottom sources then astate_n
    else
      let closure_captures =
        CapturedSources.fold
          (fun captured captures -> ClosureCaptures.add dst captured captures)
          sources astate_n.closure_captures
      in
      {astate_n with closure_captures}


  let record_closure_call loc timestamp ~callee ~actuals ~has_new_unknown_effect astate astate_n =
    if
      ClosureCaptures.is_bottom astate_n.closure_captures
      && CapturedSources.is_bottom astate_n.escaped_sources
    then astate_n
    else
      let is_lambda_constructor =
        Option.exists callee ~f:(fun pname ->
            Procname.is_constructor pname && Procname.is_lambda pname )
      in
      (* Generated lambda constructors copy their reference captures without reading the sources.
         Their bodies are not always captured, so retain this relation independently of a summary. *)
      let astate_n =
        match actuals with
        | target :: source :: _ when is_lambda_constructor ->
            record_closure_load ~src:source ~dst:target astate astate_n
        | _ ->
            astate_n
      in
      let is_copy_or_alias =
        is_lambda_constructor
        || Option.exists callee ~f:(fun pname ->
            Procname.is_destructor pname || Procname.is_std_move pname
            (* A discarded expression evaluates its address through this pure frontend builtin;
               it neither invokes nor retains the callback. *)
            || Procname.equal pname BuiltinDecl.__infer_skip
            || Option.exists (IRAttributes.load pname) ~f:(fun attrs ->
                attrs.ProcAttributes.is_cpp_copy_ctor || attrs.ProcAttributes.is_cpp_copy_assignment
                || attrs.ProcAttributes.is_cpp_move_ctor ) )
      in
      let sources =
        if is_copy_or_alias then CapturedSources.empty
        else captured_sources_of_addresses actuals astate astate_n
      in
      (* A known invocation uses its captures now. An opaque callee can also keep the callback and
       invoke it after subsequent copies, so its reference sources carry an escape obligation. *)
      let is_opaque =
        Option.value_map callee ~default:true ~f:(fun pname ->
            (not (Procname.is_cpp_lambda pname))
            && not
                 (Option.exists (IRAttributes.load pname) ~f:(fun attrs ->
                      attrs.ProcAttributes.is_defined ) ) )
      in
      let escaping_sources =
        if is_copy_or_alias then CapturedSources.empty
        else if is_opaque then sources
        else captured_sources_of_addresses ~filter:has_new_unknown_effect actuals astate astate_n
      in
      let escaping_sources = freeze_escaped_sources loc timestamp escaping_sources astate in
      let escaped_sources = CapturedSources.join astate_n.escaped_sources escaping_sources in
      let astate_n = {astate_n with escaped_sources} in
      record_capture_uses loc timestamp ~escapes:false sources astate astate_n
      |> record_capture_uses loc timestamp ~escapes:true escaped_sources astate


  let set_locked astate_n = {astate_n with locked= true}

  let is_locked {locked} = locked

  let set_load loc tstamp ident var astate_n =
    {astate_n with loads= Loads.add loc tstamp ident var astate_n.loads}


  let set_store loc timestamp var astate_n =
    {astate_n with stores= Stores.add var loc timestamp astate_n.stores}


  let get_loaded_locations var {loads} =
    Loads.get_loaded_locations var loads |> TrackedLoc.get_all_keys


  let set_passed_to loc timestamp call_exp actuals ({loads; passed_to} as astate_n) =
    let new_callee =
      match (call_exp : Exp.t) with
      | Const (Cfun callee) | Closure {name= callee} ->
          let copy_tgt =
            match actuals with
            | (tgt, _) :: _
              when Option.exists (IRAttributes.load callee) ~f:(fun attrs ->
                       attrs.ProcAttributes.is_cpp_copy_ctor
                       || attrs.ProcAttributes.is_cpp_copy_assignment
                       || attrs.ProcAttributes.is_cpp_move_ctor ) ->
                Some tgt
            | _ ->
                None
          in
          CalleeWithUnknown.V {copy_tgt; callee}
      | _ ->
          CalleeWithUnknown.Unknown
    in
    let vars =
      List.fold actuals ~init:Var.Set.empty ~f:(fun acc (actual, _) ->
          match (actual : Exp.t) with
          | Lvar pvar when not (Pvar.is_frontend_tmp pvar) ->
              Var.Set.add (Var.of_pvar pvar) acc
          | Var ident ->
              List.fold (Loads.get_all ident loads) ~init:acc ~f:(fun acc var ->
                  Var.Set.add var acc )
          | _ ->
              acc )
    in
    let passed_to =
      Var.Set.fold (fun var acc -> PassedTo.add var new_callee loc timestamp acc) vars passed_to
    in
    {astate_n with passed_to}
end

module IntraDom = struct
  include AbstractDomain.TopLifted (IntraDomElt)

  let bottom = NonTop IntraDomElt.bottom

  let is_bottom = get ~default:false IntraDomElt.is_bottom

  let mark_copy_as_modified ?reached_end ~is_modified ~copied_into ~source_addr_opt =
    map (IntraDomElt.mark_copy_as_modified ?reached_end ~is_modified ~copied_into ~source_addr_opt)


  let mark_copies_into_var_as_modified var = map (IntraDomElt.mark_copies_into_var_as_modified var)

  let mark_parameter_as_modified ~is_modified ~var =
    map (IntraDomElt.mark_parameter_as_modified ~is_modified ~var)


  let checked_via_destructor var = map (IntraDomElt.checked_via_destructor var)

  let get_copied ~ref_formals ~ptr_formals =
    get ~default:[] (IntraDomElt.get_copied ~ref_formals ~ptr_formals)


  let get_const_refable_parameters = get ~default:[] IntraDomElt.get_const_refable_parameters

  let add_var copied_into ~source_addr_opt res =
    map (IntraDomElt.add_var copied_into ~source_addr_opt res)


  let remove_var var = map (IntraDomElt.remove_var var)

  let has_intermediate_copies = get ~default:false IntraDomElt.has_intermediate_copies

  let mark_intermediates_with_shared_source actuals astate =
    map (IntraDomElt.mark_intermediates_with_shared_source actuals astate)


  let add_field copied_field ~source_addr_opt res =
    map (IntraDomElt.add_field copied_field ~source_addr_opt res)


  let add_parameter parameter_var res = map (IntraDomElt.add_parameter parameter_var res)

  let is_checked_via_destructor var = get ~default:true (IntraDomElt.is_checked_via_destructor var)

  let set_captured_variables exp = map (IntraDomElt.set_captured_variables exp)

  let record_closure_store loc timestamp ~rhs_addr ~is_escape astate =
    map (IntraDomElt.record_closure_store loc timestamp ~rhs_addr ~is_escape astate)


  let record_closure_load ~src ~dst astate = map (IntraDomElt.record_closure_load ~src ~dst astate)

  let record_closure_call loc timestamp ~callee ~actuals ~has_new_unknown_effect astate =
    map
      (IntraDomElt.record_closure_call loc timestamp ~callee ~actuals ~has_new_unknown_effect astate)


  let set_locked = map IntraDomElt.set_locked

  let is_locked = function Top -> true | NonTop elt -> IntraDomElt.is_locked elt

  let set_load loc tstamp ident var = map (IntraDomElt.set_load loc tstamp ident var)

  let set_store loc tstamp var = map (IntraDomElt.set_store loc tstamp var)

  let get_loaded_locations var = get ~default:[] (IntraDomElt.get_loaded_locations var)

  let is_captured var astate_n =
    match ((var : Var.t), (astate_n : t)) with
    | LogicalVar _, _ ->
        false
    | ProgramVar x, NonTop {captured} ->
        Captured.mem x captured
    | ProgramVar _, Top ->
        true


  let set_passed_to loc timestamp call_exp actuals =
    map (IntraDomElt.set_passed_to loc timestamp call_exp actuals)


  let get_passed_to var ~f = function
    | Top ->
        `Top
    | NonTop {IntraDomElt.passed_to} ->
        let callees =
          PassedTo.find var passed_to |> CalleesWithLoc.filter (fun callee _ -> f callee)
        in
        `PassedTo callees


  let is_lifetime_extended var astate_n =
    is_captured var astate_n
    ||
    match
      get_passed_to var astate_n ~f:(function
        | CalleeWithUnknown.V {callee} ->
            not (Procname.is_shared_ptr_observer callee)
        | CalleeWithUnknown.Unknown ->
            true )
    with
    | `Top ->
        true
    | `PassedTo callees ->
        not (CalleesWithLoc.is_empty callees)
end

module InterDom = struct
  include AbstractDomain.TopLifted (TransitiveInfo)

  let bottom = NonTop TransitiveInfo.bottom

  let is_bottom = get ~default:false TransitiveInfo.is_bottom

  let remember_dropped_elements dropped = map (TransitiveInfo.remember_dropped_elements ~dropped)

  let add_specialized_direct_callee procname specialisation loc =
    map (TransitiveInfo.add_specialized_direct_callee procname specialisation loc)


  let yojson_of_t info_or_top =
    let info = match info_or_top with Top -> TransitiveInfo.bottom | NonTop info -> info in
    TransitiveInfo.yojson_of_t info


  let apply_summary ~callee_pname ~call_loc ~summary ~skip_transitive_accesses non_disj =
    match (non_disj, summary) with
    | Top, _ | _, Top ->
        Top
    | NonTop non_disj, NonTop summary ->
        let non_disj =
          if skip_transitive_accesses then non_disj
          else TransitiveInfo.apply_summary ~callee_pname ~call_loc ~summary non_disj
        in
        NonTop non_disj
end

module OverApproxDomain = struct
  include AbstractDomain.BottomLifted (struct
    type t = AbductiveDomain.t * PathContext.t

    let pp fmt pair = Pp.pair ~fst:AbductiveDomain.pp ~snd:PathContext.pp fmt pair

    let leq ~lhs:(astate_lhs, _) ~rhs:(astate_rhs, _) =
      AbductiveDomain.leq ~lhs:astate_lhs ~rhs:astate_rhs


    let join = PulseJoin.join

    let widen ~prev ~next ~num_iters:_ = join prev next
  end)

  (* this is dodgy but we won't need this for long (famous last words...)  and it behaves the way we
     want for now: when we reach top and start joining with a non-top, non-bottom abstract state,
     then the result is that abstract state again and not top, which means we start "remembering"
     (and joining) abstract states even after reaching top. We never really reach top anyway, top is
     used by {!AbstractInterpreter} as a hack for when there are no executable disjuncts
     (so... bottom!) *)
  let top = bottom

  (* *cough*... see [top] above *)
  let is_top = is_bottom

  let join lhs rhs = if Config.pulse_over_approximate_reasoning then join lhs rhs else bottom

  let widen ~prev ~next ~num_iters =
    if Config.pulse_over_approximate_reasoning then widen ~prev ~next ~num_iters else bottom


  let pp_with_kind pp_kind fmt astate_path =
    let pp fmt (astate, path) = PulsePp.pp pp_kind (Some path) fmt astate in
    AbstractDomain.BottomLiftedUtils.pp pp fmt astate_path
end

type t =
  { intra: IntraDom.t
  ; inter: InterDom.t
  ; has_dropped_disjuncts: AbstractDomain.BooleanOr.t
  ; astate: OverApproxDomain.t }
[@@deriving abstract_domain]

let pp_with_kind pp_kind fmt
    ({intra; inter; has_dropped_disjuncts; astate} [@warning "missing-record-field-pattern"]) =
  F.fprintf fmt "@[over-approx: %a@\n%a,@ %a%s@]"
    (OverApproxDomain.pp_with_kind pp_kind)
    astate IntraDom.pp intra InterDom.pp inter
    (if has_dropped_disjuncts then " (some disjuncts dropped)" else "")


let pp fmt non_disj = pp_with_kind TEXT fmt non_disj

let bottom =
  {intra= IntraDom.bottom; inter= InterDom.bottom; has_dropped_disjuncts= false; astate= Bottom}


let is_bottom
    ({intra; inter; has_dropped_disjuncts; astate} [@warning "missing-record-field-pattern"]) =
  IntraDom.is_bottom intra && InterDom.is_bottom inter
  && AbstractDomain.BooleanOr.is_bottom has_dropped_disjuncts
  && OverApproxDomain.is_bottom astate


let top =
  { intra= IntraDom.top
  ; inter= InterDom.top
  ; has_dropped_disjuncts= true
  ; astate= OverApproxDomain.top }


let is_top ({intra; inter; has_dropped_disjuncts; astate} [@warning "missing-record-field-pattern"])
    =
  IntraDom.is_top intra && InterDom.is_top inter && has_dropped_disjuncts
  && OverApproxDomain.is_top astate


let top_keeping_dropped_disjuncts ({has_dropped_disjuncts} [@warning "missing-record-field-pattern"])
    =
  {top with has_dropped_disjuncts}


(* faster? *)
let join lhs rhs = if is_bottom lhs then rhs else if is_bottom rhs then lhs else join lhs rhs

let map_intra f ({intra} as x) = {x with intra= f intra}

let map_inter f ({inter} as x) = {x with inter= f inter}

let mark_copy_as_modified ?reached_end ~is_modified ~copied_into ~source_addr_opt =
  map_intra (IntraDom.mark_copy_as_modified ?reached_end ~is_modified ~copied_into ~source_addr_opt)


let mark_copies_into_var_as_modified var = map_intra (IntraDom.mark_copies_into_var_as_modified var)

let mark_parameter_as_modified ~is_modified ~var =
  map_intra (IntraDom.mark_parameter_as_modified ~is_modified ~var)


let checked_via_destructor var = map_intra (IntraDom.checked_via_destructor var)

let get_copied ~ref_formals ~ptr_formals {intra} =
  IntraDom.get_copied ~ref_formals ~ptr_formals intra


let get_const_refable_parameters {intra} = IntraDom.get_const_refable_parameters intra

let add_var copied_into ~source_addr_opt res =
  map_intra (IntraDom.add_var copied_into ~source_addr_opt res)


let remove_var var = map_intra (IntraDom.remove_var var)

let has_intermediate_copies {intra} = IntraDom.has_intermediate_copies intra

let mark_intermediates_with_shared_source actuals astate =
  map_intra (IntraDom.mark_intermediates_with_shared_source actuals astate)


let add_field copied_field ~source_addr_opt res =
  map_intra (IntraDom.add_field copied_field ~source_addr_opt res)


let add_parameter parameter_var res = map_intra (IntraDom.add_parameter parameter_var res)

let is_checked_via_destructor var {intra} = IntraDom.is_checked_via_destructor var intra

let set_captured_variables exp = map_intra (IntraDom.set_captured_variables exp)

let record_closure_store loc timestamp ~rhs_addr ~is_escape astate =
  map_intra (IntraDom.record_closure_store loc timestamp ~rhs_addr ~is_escape astate)


let record_closure_load ~src ~dst astate = map_intra (IntraDom.record_closure_load ~src ~dst astate)

let record_closure_call loc timestamp ~callee ~actuals ~has_new_unknown_effect astate =
  map_intra
    (IntraDom.record_closure_call loc timestamp ~callee ~actuals ~has_new_unknown_effect astate)


let set_locked = map_intra IntraDom.set_locked

let is_locked {intra} = IntraDom.is_locked intra

let set_load loc tstamp ident var astate_n =
  if Ident.is_none ident then astate_n
  else map_intra (IntraDom.set_load loc tstamp ident var) astate_n


let set_store loc tstamp var = map_intra (IntraDom.set_store loc tstamp var)

let get_loaded_locations var {intra} = IntraDom.get_loaded_locations var intra

let set_passed_to loc timestamp call_exp actuals =
  map_intra (IntraDom.set_passed_to loc timestamp call_exp actuals)


let is_lifetime_extended var {intra} = IntraDom.is_lifetime_extended var intra

let remember_dropped_disjuncts disjuncts non_disj =
  let non_disj =
    if List.is_empty disjuncts then non_disj else {non_disj with has_dropped_disjuncts= true}
  in
  List.fold disjuncts ~init:non_disj ~f:(fun non_disj (exec, path) ->
      match exec with
      | ExecutionDomain.ContinueProgram astate ->
          let inter =
            InterDom.remember_dropped_elements astate.AbductiveDomain.transitive_info non_disj.inter
          in
          let astate = OverApproxDomain.join (NonBottom (astate, path)) non_disj.astate in
          {non_disj with inter; astate}
      | _ ->
          non_disj )


let add_specialized_direct_callee procname specialisation loc non_disj =
  map_inter (InterDom.add_specialized_direct_callee procname specialisation loc) non_disj


let bind (execs, non_disj) ~f =
  List.rev execs
  |> List.fold ~init:([], bottom) ~f:(fun (acc, joined_non_disj) elt ->
      let l, new_non_disj = f elt non_disj in
      (l @ acc, join joined_non_disj new_non_disj) )


(* start executing each disjunct with an empty non-disjunctive domain; the non-disjunctive state
   before the instruction will be executed separately by [exec] below from [AbstractInterpreter]
   calling Pulse.ml's [exec_instr_non_disj] *)
let for_disjunct_exec_instr non_disj = {non_disj with astate= Bottom}

let exec non_disj ~exec_instr =
  if Config.pulse_over_approximate_reasoning then
    match non_disj.astate with
    | Bottom ->
        bottom
    | NonBottom (astate, path) ->
        (* move the abstract state from the non-disjunctive part into a single disjunct *)
        let non_disj = {non_disj with astate= Bottom} in
        (* set the context to "non-disjunctive" *)
        let path = PathContext.set_is_non_disj path in
        let exec_states_paths, non_disj =
          exec_instr ((ExecutionDomain.ContinueProgram astate, path), non_disj)
        in
        let astate =
          List.fold exec_states_paths ~init:Bottom ~f:(fun astate_join (exec_state, path) ->
              match (exec_state : ExecutionDomain.t) with
              | ContinueProgram astate ->
                  OverApproxDomain.join astate_join (NonBottom (astate, path))
              | _ ->
                  (* TODO: we may want separate over-approximate states for other kinds of
                     disjuncts, especially for exceptional disjuncts *)
                  astate_join )
        in
        if OverApproxDomain.is_bottom astate then bottom else {non_disj with astate}
  else non_disj


let join_to_astate astate non_disj =
  {non_disj with astate= OverApproxDomain.join non_disj.astate astate}


let astate_is_bottom {astate} = OverApproxDomain.is_bottom astate

type summary =
  { transitive_info: InterDom.t
  ; has_dropped_disjuncts: AbstractDomain.BooleanOr.t
  ; astate: AbductiveDomain.Summary.t AbstractDomain.Types.bottom_lifted }

let make_summary proc_attributes location
    ({inter= transitive_info; has_dropped_disjuncts; astate} : t) =
  let astate =
    match astate with
    | Bottom ->
        Bottom
    | NonBottom (astate, _path) -> (
      match AbductiveDomain.Summary.of_post proc_attributes location astate with
      | Sat (Ok summary) ->
          NonBottom summary
      | _ ->
          (* TODO(join): we should be more lenient here *) Bottom )
  in
  {transitive_info; has_dropped_disjuncts; astate}


let apply_summary ~callee_pname ~call_loc ~skip_transitive_accesses (non_disj : t) summary =
  let non_disj =
    { non_disj with
      has_dropped_disjuncts=
        AbstractDomain.BooleanOr.join non_disj.has_dropped_disjuncts summary.has_dropped_disjuncts
    }
  in
  map_inter
    (InterDom.apply_summary ~callee_pname ~call_loc ~skip_transitive_accesses
       ~summary:summary.transitive_info )
    non_disj


module Summary = struct
  type t = summary

  let yojson_of_t {transitive_info} = InterDom.yojson_of_t transitive_info

  let pp fmt {transitive_info; has_dropped_disjuncts; astate} =
    F.fprintf fmt "over-approx:@[%a@]@\n%a%s"
      (AbstractDomain.BottomLiftedUtils.pp AbductiveDomain.Summary.pp)
      astate InterDom.pp transitive_info
      (if has_dropped_disjuncts then " (some disjuncts dropped)" else "")


  let bottom =
    { transitive_info= InterDom.bottom
    ; has_dropped_disjuncts= AbstractDomain.BooleanOr.bottom
    ; astate= Bottom }


  let join
      { transitive_info= transitive_info1
      ; has_dropped_disjuncts= has_dropped_disjuncts1
      ; astate= astate1 }
      { transitive_info= transitive_info2
      ; has_dropped_disjuncts= has_dropped_disjuncts2
      ; astate= astate2 } =
    { transitive_info= InterDom.join transitive_info1 transitive_info2
    ; has_dropped_disjuncts=
        AbstractDomain.BooleanOr.join has_dropped_disjuncts1 has_dropped_disjuncts2
    ; astate= AbstractDomain.BottomLiftedUtils.join PulseJoin.join_summaries astate1 astate2 }


  let get_transitive_info_if_not_top {transitive_info} =
    match transitive_info with Top -> None | NonTop transitive_info -> Some transitive_info


  let has_dropped_disjuncts {has_dropped_disjuncts} = has_dropped_disjuncts

  let get_pre_post {astate} = astate
end
