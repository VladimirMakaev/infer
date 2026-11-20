(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)
open! IStd
module F = Format
module L = Logging
module MF = MarkupFormatter

type access = HilExp.t option MemoryAccess.t [@@deriving compare, equal]

let get_pointee_typ (typ : Typ.t) =
  match typ.desc with
  | Tptr (typ, _) ->
      Some typ
  | Tstruct name when Typ.is_unique_pointer typ || Typ.is_shared_pointer typ -> (
    match Typ.Name.get_template_spec_info name with
    | Some (Typ.Template {args= TType typ :: _}) ->
        Some typ
    | _ ->
        None )
  | _ ->
      None


let get_access_typ tenv prev_typ (access : access) =
  let lookup tn = Tenv.lookup tenv tn in
  match access with
  | FieldAccess field_name ->
      Struct.get_field_type_and_annotation ~lookup field_name prev_typ |> Option.map ~f:fst
  | ArrayAccess (typ, _) ->
      Some typ
  | TakeAddress ->
      Some (Typ.mk (Tptr (prev_typ, Pk_pointer)))
  | Dereference ->
      get_pointee_typ prev_typ


type access_list = access list [@@deriving compare, equal]

let get_path_typ tenv (base_typ, accesses) =
  let f acc access = match acc with Some typ -> get_access_typ tenv typ access | None -> None in
  List.fold accesses ~init:(Some base_typ) ~f


(* classes of the objects whose fields [accesses] access, starting from a value of type [typ] *)
let rec classes_of_objects tenv typ (accesses : access_list) =
  match accesses with
  | [] ->
      []
  | access :: rest -> (
      let classes =
        get_access_typ tenv typ access
        |> Option.value_map ~default:[] ~f:(fun typ -> classes_of_objects tenv typ rest)
      in
      match (typ.Typ.desc, access) with
      | Tstruct name, FieldAccess _ ->
          name :: classes
      | _ ->
          classes )


let normalise_access_list (accesses : access_list) =
  let exception NormalisationFailure in
  let rec normalise_access_list_inner (accesses : access_list) =
    match accesses with
    | [] ->
        []
    | TakeAddress :: Dereference :: rest ->
        normalise_access_list_inner rest
    | (TakeAddress as _addr) :: (FieldAccess _ as field) :: rest ->
        field :: normalise_access_list_inner rest
    | TakeAddress :: _ :: _ ->
        (* an address can only be dereferenced *)
        raise NormalisationFailure
    | access :: rest ->
        access :: normalise_access_list_inner rest
  in
  try Some (normalise_access_list_inner accesses) with NormalisationFailure -> None


let pp_with_base pp_base fmt (base, accesses) =
  let rec pp_rev_accesses fmt (accesses : access_list) =
    match (accesses, Language.get_language ()) with
    | _, Erlang ->
        L.internal_error "Erlang not supported@\n"
    | _, Hack ->
        L.internal_error "Hack not supported@\n"
    | _, Python ->
        L.internal_error "Python not supported@\n"
    | _, Rust ->
        L.internal_error "Rust not supported@\n"
    | _, Swift ->
        L.internal_error "Swift not supported@\n"
    | [], _ ->
        pp_base fmt base
    | ArrayAccess _ :: rest, _ ->
        F.fprintf fmt "%a[]" pp_rev_accesses rest
    | FieldAccess field_name :: Dereference :: rest, _ ->
        let op =
          match Language.get_language () with
          | Clang ->
              "->"
          | Java ->
              "."
          | CIL ->
              "."
          | Erlang ->
              L.die InternalError "Erlang not supported"
          | Hack ->
              L.die InternalError "Hack not supported"
          | Python ->
              L.die InternalError "Python not supported"
          | Rust ->
              L.die InternalError "Rust not supported"
          | Swift ->
              L.die InternalError "Swift not supported"
        in
        F.fprintf fmt "%a%s%a" pp_rev_accesses rest op Fieldname.pp field_name
    | FieldAccess field_name :: rest, _ ->
        (* Java is allowed here only because the frontend is broken and generates
           [FieldAccess] without a [Dereference] for static fields *)
        F.fprintf fmt "%a.%a" pp_rev_accesses rest Fieldname.pp field_name
    | Dereference :: rest, Clang ->
        F.fprintf fmt "*(%a)" pp_rev_accesses rest
    | TakeAddress :: rest, Clang ->
        F.fprintf fmt "&(%a)" pp_rev_accesses rest
    | Dereference :: rest, CIL ->
        F.fprintf fmt "*(%a)" pp_rev_accesses rest
    | TakeAddress :: rest, CIL ->
        F.fprintf fmt "&(%a)" pp_rev_accesses rest
    | access :: rest, Java ->
        L.internal_error "Asked to print %a in Java mode@\n"
          (MemoryAccess.pp (fun _ _ -> ()))
          access ;
        pp_rev_accesses fmt rest
  in
  pp_rev_accesses fmt (List.rev accesses)


let global_key pvar =
  ( Pvar.get_name pvar
  , Pvar.get_template_args pvar
  , if Pvar.is_static_global pvar then Pvar.get_translation_unit pvar else None )


(* A wrapper that ignores ProgramVar.Global_var translation_unit in comparison, except for static
 * variables, as we cannot add that ignore there due to issues with Siof
 * similar hack to D51588007 *)
module SVar = struct
  include Var

  let compare x y =
    match (x, y) with
    | ProgramVar x, ProgramVar y when Pvar.is_global x && Pvar.is_global y ->
        [%compare: Mangled.t * Typ.template_spec_info * SourceFile.t option] (global_key x)
          (global_key y)
    | ProgramVar x, _ when Pvar.is_global x ->
        -1
    | _, ProgramVar x when Pvar.is_global x ->
        1
    | _ ->
        Var.compare x y


  let equal x y =
    match (x, y) with
    | ProgramVar x, ProgramVar y when Pvar.is_global x && Pvar.is_global y ->
        [%equal: Mangled.t * Typ.template_spec_info * SourceFile.t option] (global_key x)
          (global_key y)
    | ProgramVar x, _ when Pvar.is_global x ->
        false
    | _, ProgramVar x when Pvar.is_global x ->
        false
    | _ ->
        Var.equal x y
end

(** var type used only for printing, not comparisons *)
module IgnoreVar = struct
  type t = Var.t

  let compare _x _y = 0

  let equal _x _y = true
end

type raw_path = (SVar.t * Typ.t) * access_list [@@deriving compare, equal]

(** path-like type using [MemoryAccess] steps instead of [AccessPath.access]. It does not ignore the
    root variable type (like the original [AccessPath.t]) but instead ignores the root variable for
    comparisons. *)
type unrooted_path = (IgnoreVar.t * Typ.t) * access_list [@@deriving compare, equal]

type t =
  | Global of {path: raw_path}  (** root var is included in comparison *)
  | Class of {typename: Typ.Name.t}  (** Java-only class object identified by typename *)
  | Parameter of {index: int; path: unrooted_path}
      (** method parameter represented by its 0-indexed position, root var is not used in comparison
      *)
  | Opaque of {path: unrooted_path}
      (** reached from a value the caller cannot name, eg returned by a call, root var is not used
          in comparison *)
[@@deriving compare, equal]

let get_typ tenv = function
  | Class _ ->
      Some StdTyp.Java.pointer_to_java_lang_class
  | Global {path= (_, typ), accesses}
  | Parameter {path= (_, typ), accesses}
  | Opaque {path= (_, typ), accesses} ->
      get_path_typ tenv (typ, accesses)


let append ~on_to:(base, accesses) (_, accesses') =
  match normalise_access_list (accesses @ accesses') with
  | Some accesses'' ->
      Some (base, accesses'')
  | None ->
      None


let address_of_field tenv t field =
  let path_of_field (base, accesses) =
    get_typ tenv t
    |> Option.bind ~f:(fun (typ : Typ.t) ->
        let extra =
          match typ.desc with
          | Tptr ({desc= Tstruct _}, _) ->
              Some
                [MemoryAccess.Dereference; MemoryAccess.FieldAccess field; MemoryAccess.TakeAddress]
          | Tstruct _ ->
              Some [MemoryAccess.FieldAccess field; MemoryAccess.TakeAddress]
          | _ ->
              None
        in
        Option.bind extra ~f:(fun extra ->
            normalise_access_list (accesses @ extra)
            |> Option.map ~f:(fun accesses -> (base, accesses)) ) )
  in
  match t with
  | Parameter param ->
      Option.map (path_of_field param.path) ~f:(fun path -> Parameter {param with path})
  | Global global ->
      Option.map (path_of_field global.path) ~f:(fun path -> Global {path})
  | Opaque opaque ->
      Option.map (path_of_field opaque.path) ~f:(fun path -> Opaque {path})
  | Class _ ->
      None


(* Remove initial prefix of synthetic java fields giving access to outer-class fields.
   This allows comparing as equal two paths generated by two different inner classes of the
   same outer class. *)
let rec inner_class_normalise tenv ((typ, (accesses : access_list)) as path) =
  match accesses with
  | (Dereference as access) :: (FieldAccess fieldname as access') :: rest
    when Fieldname.is_java_outer_instance fieldname -> (
    match get_access_typ tenv typ access with
    | Some typ' -> (
      match get_access_typ tenv typ' access' with
      | Some typ'' ->
          inner_class_normalise tenv (typ'', rest)
      | None ->
          path )
    | None ->
        path )
  | _ ->
      path


let equal_across_threads tenv t1 t2 =
  match (t1, t2) with
  | ( (Parameter {path= (_, typ1), accesses1} | Opaque {path= (_, typ1), accesses1})
    , (Parameter {path= (_, typ2), accesses2} | Opaque {path= (_, typ2), accesses2}) ) ->
      (* parameter position/names can be ignored across threads, if accesses and types modulo
         qualifiers are equal ([this] is [C const *] in const methods) *)
      let typ1, accesses1 = inner_class_normalise tenv (typ1, accesses1) in
      let typ2, accesses2 = inner_class_normalise tenv (typ2, accesses2) in
      Typ.equal_ignore_quals typ1 typ2 && equal_access_list accesses1 accesses2
  | _, _ ->
      (* globals and class objects must be identical across threads *)
      equal t1 t2


let root_object (typ, (accesses : access_list)) =
  match (typ.Typ.desc, accesses) with
  | Tptr ({desc= Tstruct name}, _), Dereference :: (FieldAccess _ :: _ as rest) ->
      Some (name, rest)
  | _ ->
      None


(* does the path reach an object of class [name] and then perform exactly [accesses] on it? The
   object must be the first of its class on the path: the next ones are usually linked to it, like
   the nodes of a list or a tree, and matching them too would report a deadlock between methods
   that lock such nodes in the same order. Returns whether getting to the object traverses a field,
   ie whether it may be an object OTHER than the root of the path: a prefix of dereferences only,
   as when a [T&] parameter is matched against the [this] of a method of [T], reaches the root
   itself *)
let reaches_object_through tenv (typ, (path_accesses : access_list)) (name, accesses) =
  let prefix_length = List.length path_accesses - List.length accesses in
  if prefix_length <= 0 then None
  else
    let prefix, suffix = List.split_n path_accesses prefix_length in
    if
      equal_access_list suffix accesses
      && (not (List.mem (classes_of_objects tenv typ prefix) name ~equal:Typ.Name.equal))
      && Option.bind (get_path_typ tenv (typ, prefix)) ~f:Typ.name
         |> Option.exists ~f:(Typ.Name.equal name)
    then Some (List.exists prefix ~f:(function MemoryAccess.FieldAccess _ -> true | _ -> false))
    else None


type alias_direction = SameObject | SecondUnderFirst | FirstUnderSecond

let may_alias_across_threads_direction tenv t1 t2 =
  if equal_across_threads tenv t1 t2 then Some SameObject
  else
    match (t1, t2) with
    | Parameter {path= (_, typ1), accesses1}, Parameter {path= (_, typ2), accesses2} -> (
        let path1 = inner_class_normalise tenv (typ1, accesses1) in
        let path2 = inner_class_normalise tenv (typ2, accesses2) in
        let through_field direction = function
          | true ->
              Some direction
          | false ->
              (* no field was traversed, so the two roots are the same object *)
              Some SameObject
        in
        match Option.bind (root_object path2) ~f:(reaches_object_through tenv path1) with
        | Some reached ->
            through_field SecondUnderFirst reached
        | None ->
            Option.bind (root_object path1) ~f:(reaches_object_through tenv path2)
            |> Option.bind ~f:(through_field FirstUnderSecond) )
    | _, _ ->
        None


let may_alias_across_threads tenv t1 t2 =
  may_alias_across_threads_direction tenv t1 t2 |> Option.is_some


let normalise_across_threads tenv t =
  match t with
  | Parameter {path= (var, typ), accesses} | Opaque {path= (var, typ), accesses} ->
      (* only the last field is kept, so any relation that preserves it stays within one key *)
      let _, accesses = inner_class_normalise tenv (typ, accesses) in
      let last_field =
        List.rev accesses
        |> List.find ~f:(fun (access : access) ->
            match access with FieldAccess _ -> true | _ -> false )
        |> Option.to_list
      in
      Parameter {index= 0; path= ((var, StdTyp.void), last_field)}
  | Global _ | Class _ ->
      t


let is_class_object = function Class _ -> true | _ -> false

let may_denote_distinct_objects = function
  | Global {path= _, accesses} | Parameter {path= _, accesses} ->
      List.exists accesses ~f:(function MemoryAccess.ArrayAccess _ -> true | _ -> false)
  | Opaque _ ->
      true
  | Class _ ->
      false


let get_last_field_or_global = function
  | Global {path= (var, _), accesses}
  | Parameter {path= (var, _), accesses}
  | Opaque {path= (var, _), accesses} -> (
      let last_field =
        List.fold accesses ~init:None ~f:(fun last (access : access) ->
            match access with FieldAccess field -> Some field | _ -> last )
      in
      match (last_field, (var : Var.t)) with
      | Some field, _ ->
          Some (First field)
      | None, ProgramVar pvar when Pvar.is_global pvar ->
          Some (Second pvar)
      | None, _ ->
          None )
  | Class _ ->
      None


(** The object a reference-typed global was bound to, if its initialiser says so unambiguously.

    A C++ reference cannot be rebound, so the store in the initialiser of a reference-typed global
    names the object that reference denotes for the whole run. Without resolving it the reference
    variable becomes a lock of its own -- [G{(rb:std::mutex&)}] for [std::mutex& rb = b] -- which
    can never be equal to [G{&b}], so a lock order spelled through the reference is invisible and
    the inversion is not reported.

    A POINTER is deliberately left unresolved. [Typ.is_reference] holds only for
    [Pk_lvalue_reference] and [Pk_rvalue_reference], not for [Pk_pointer], and that is the whole
    distinction being relied on here: a pointer can be assigned again after its initialiser runs, so
    what it points to where the lock is taken does not follow from how it was initialised.

    Anything but a single store of one global's address gives up. Two stores of different objects,
    or a store of something this does not read, leave the address alone rather than pick one: a
    wrong resolution would merge two unrelated locks, which invents a deadlock. *)
let referent_cache : Pvar.t option Procname.Cache.t =
  Procname.Cache.create ~name:"starvation_reference_referent"


(** Read one procedure's instructions once: every load, and for each program variable the single
    expression stored to it. A variable stored twice with different expressions maps to [None],
    because naming one of the two would be a guess and a wrong one merges unrelated locks.

    Both binding sites use this. A reference GLOBAL is bound in its own initialiser procedure, one
    reference per procedure; a block-local reference is bound in the procedure that declares it,
    which declares many, so the result has to be read per variable rather than per procedure. *)
let read_bindings pname =
  Procdesc.load pname
  |> Option.map ~f:(fun pdesc ->
      Procdesc.fold_instrs pdesc ~init:(Ident.Map.empty, Pvar.Map.empty)
        ~f:(fun (loads, stores) _ (instr : Sil.instr) ->
          match instr with
          | Load {id; e} ->
              (Ident.Map.add id e loads, stores)
          | Store {e1= Lvar lhs; e2; typ} ->
              let merge = function
                | None ->
                    Some (Some (e2, typ))
                | Some (Some (previous, _)) as unchanged when Exp.equal previous e2 ->
                    unchanged
                | Some _ ->
                    Some None
              in
              (loads, Pvar.Map.update lhs merge stores)
          | _ ->
              (loads, stores) ) )


(** the variable whose address was stored, following one load.

    [std::mutex& r = other_ref] does not store an address directly: it LOADS the value of
    [other_ref] and stores that, so the stored expression is an identifier. Following the load back
    to the reference it came from, and resolving that in turn with [follow], is what lets a
    reference bound to another reference reach the mutex at the end of the chain. *)
let stored_referent ~loads ~follow stored =
  match (stored : (Exp.t * Typ.t) option) with
  | Some (Lvar referent, _) ->
      Some referent
  | Some (Var id, _) -> (
    match Ident.Map.find_opt id loads with
    | Some (Exp.Lvar loaded_from) ->
        follow loaded_from
    | _ ->
        None )
  | _ ->
      None


(* [Pvar.Map.find_opt] absent means no store at all and [Some None] means two conflicting ones;
   both must give up, so they are collapsed here. *)
let stored_in stores pvar = Pvar.Map.find_opt pvar stores |> Option.join

(* Taking the address of a variable and taking the address of a pointer variable produce the same
   [Lvar] in SIL, so the stored expression alone cannot tell [std::mutex& r = m] from
   [std::mutex* p = &m]. The store's type can: it is the type of [*e1], which is a reference for
   the first and [Pk_pointer] for the second. Only a reference may be followed -- a pointer can be
   assigned again after its initialiser, so what it points to where the lock is taken does not
   follow from how it was initialised. *)
let binding_is_reference stored =
  match (stored : (Exp.t * Typ.t) option) with
  | Some (_, typ) ->
      Typ.is_reference typ
  | None ->
      false


let rec referent_of_reference_global ~visiting pvar =
  if Pvar.Set.mem pvar visiting then None
  else
    let visiting = Pvar.Set.add pvar visiting in
    Pvar.get_initializer_pname pvar
    |> Option.bind ~f:(fun pname ->
        (* A cycle cannot arise from valid C++ here, since each initialiser reads a reference
           that is already bound, so the guard above is defensive only and caching its result
           costs at worst a report. Keying on the initialiser procedure is sound because it
           binds exactly this one reference. *)
        match Procname.Cache.lookup referent_cache pname with
        | Some referent ->
            referent
        | None ->
            let referent =
              read_bindings pname
              |> Option.bind ~f:(fun (loads, stores) ->
                  let stored = stored_in stores pvar in
                  (* gated here rather than at the call site so that every caller, including the
                     block-local chain below, inherits the pointer exclusion *)
                  if binding_is_reference stored then
                    stored_referent ~loads ~follow:(referent_of_reference_global ~visiting) stored
                  else None )
            in
            Procname.Cache.add referent_cache pname referent ;
            referent )


let referent_of_reference_global pvar = referent_of_reference_global ~visiting:Pvar.Set.empty pvar

let local_bindings_cache : Pvar.t Pvar.Map.t Procname.Cache.t =
  Procname.Cache.create ~name:"starvation_reference_local_bindings"


(** For every variable bound in [pname], the GLOBAL its chain of references ends at.

    One procedure declares many references, so unlike [referent_of_reference_global] this cannot
    cache a single answer per procedure: every reference in it would collide on that entry and take
    whichever was resolved first, silently merging unrelated locks. The whole map is built in one
    scan and cached instead, which also means a chain of block-local references resolves without
    re-reading the procedure. *)
let local_bindings pname =
  match Procname.Cache.lookup local_bindings_cache pname with
  | Some bindings ->
      bindings
  | None ->
      let bindings =
        read_bindings pname
        |> Option.value_map ~default:Pvar.Map.empty ~f:(fun (loads, stores) ->
            (* A global reached here may itself be a reference-typed global, which has its own
               binding in its own initialiser. Stopping at the first global would name THAT
               variable, and since its type is still a reference the later [resolve_reference]
               would resolve it again and append a second address-of, giving [&(&(t))], which
               matches nothing and silently drops the inversion. Following the chain to the end
               here means exactly one address-of is ever taken. [referent_of_reference_global]
               returns [None] for a global that is not a reference, which is what ends the loop
               and what keeps pointer globals excluded. *)
            let rec through_reference_globals ~visiting pvar =
              if Pvar.Set.mem pvar visiting then Some pvar
              else
                match referent_of_reference_global pvar with
                | Some referent ->
                    through_reference_globals ~visiting:(Pvar.Set.add pvar visiting) referent
                | None ->
                    Some pvar
            in
            let rec resolve ~visiting pvar =
              if Pvar.is_global pvar then through_reference_globals ~visiting:Pvar.Set.empty pvar
              else if Pvar.Set.mem pvar visiting then None
              else
                let visiting = Pvar.Set.add pvar visiting in
                stored_referent ~loads ~follow:(resolve ~visiting) (stored_in stores pvar)
                |> Option.bind ~f:(resolve ~visiting)
            in
            Pvar.Map.filter_map (fun pvar _ -> resolve ~visiting:Pvar.Set.empty pvar) stores )
      in
      Procname.Cache.add local_bindings_cache pname bindings ;
      bindings


(** the global a block-local reference is bound to, if its binding settles on one *)
let referent_of_local_reference pvar =
  Pvar.get_declaring_function pvar
  |> Option.bind ~f:(fun pname -> Pvar.Map.find_opt pvar (local_bindings pname))


(** Rewrite a lock taken through a reference-typed global into the same address the referent's own
    locks use, so the two compare equal.

    [rb] carrying accesses [accesses] denotes the referent, while a lock on [b] directly is its
    ADDRESS, [&b]. The rewritten path is therefore the referent with an address-of underneath the
    accesses already there. This list is outermost-first, so underneath means at the end. *)
let resolve_reference address =
  match address with
  | Global {path= (Var.ProgramVar pvar, typ), accesses} when Typ.is_reference typ ->
      referent_of_reference_global pvar
      |> Option.value_map ~default:address ~f:(fun referent ->
          let address_of : access = MemoryAccess.TakeAddress in
          (* [typ] is carried over rather than stripped to the pointee. Taking a lock on [b]
             directly reaches here as [&b] with base type [std::mutex&] -- the type of the
             argument, not of the object -- so the resolved path has to spell it the same way or
             the two addresses still compare unequal and nothing is gained. *)
          Global {path= ((Var.of_pvar referent, typ), accesses @ [address_of])} )
  | Global _ | Parameter _ | Opaque _ | Class _ ->
      address


let rec make formal_map (hilexp : HilExp.t) =
  let make_from_acc_exp acc_exp =
    match HilExp.AccessExpression.to_accesses acc_exp with
    | HilExp.AccessExpression.Base ((var, typ) as base), accesses -> (
      match normalise_access_list accesses with
      | Some accesses -> (
          let path = (base, accesses) in
          match var with
          | Var.LogicalVar _ ->
              (* ignore logical variables *)
              None
          | Var.ProgramVar pvar when Pvar.is_global pvar ->
              Some (Global {path})
          | Var.ProgramVar pvar -> (
            match FormalMap.get_formal_index base formal_map with
            | Some index ->
                Some (Parameter {index; path})
            | None ->
                (* Not a formal, so there is no address for it and the acquisition used to be
                   dropped here. A block-local REFERENCE is still worth naming: it cannot be
                   rebound, so the single store in its own procedure settles what it denotes, and
                   when that is a global the lock is the global's own. The address is spelled the
                   way [resolve_reference] spells a resolved global -- the referent carrying the
                   reference type, with an address-of underneath the accesses -- so that locking
                   the global directly and locking it through this reference compare equal.

                   Anything else stays dropped: a non-reference local has no address to speak of,
                   a pointer can be reassigned after its initialiser, and a reference bound to a
                   field or to another local never reaches a global. *)
                if Typ.is_reference typ then
                  referent_of_local_reference pvar
                  |> Option.map ~f:(fun referent ->
                      let address_of : access = MemoryAccess.TakeAddress in
                      Global {path= ((Var.of_pvar referent, typ), accesses @ [address_of])} )
                else None ) )
      | _ ->
          None )
    | _ ->
        None
  in
  match hilexp with
  | AccessExpression access_exp ->
      make_from_acc_exp access_exp
  | Constant (Cclass class_id) ->
      (* this is a synchronized(CLASSNAME.class) or class object construct *)
      let typename = Ident.name_to_string class_id |> Typ.Name.Java.from_string in
      Some (Class {typename})
  | Cast (_, hilexp) | Exception hilexp | UnaryOperator (_, hilexp, _) ->
      make formal_map hilexp
  | BinaryOperator _ | Closure _ | Constant _ | Sizeof _ ->
      None


let pp fmt t =
  let pp_path fmt path =
    let pp_base fmt (var, typ) = F.fprintf fmt "(%a:%a)" Var.pp var (Typ.pp_full Pp.text) typ in
    pp_with_base pp_base fmt path
  in
  match t with
  | Global {path} ->
      F.fprintf fmt "G{%a}" pp_path path
  | Class {typename} ->
      F.fprintf fmt "C{%s}" (Typ.Name.name typename)
  | Parameter {index; path} ->
      F.fprintf fmt "P<%i>{%a}" index pp_path path
  | Opaque {path} ->
      F.fprintf fmt "O{%a}" pp_path path


let root_class = function
  | Class {typename} ->
      Some typename
  | Global {path= (_, {desc}), _} | Parameter {path= (_, {desc}), _} | Opaque {path= (_, {desc}), _}
    -> (
    match desc with
    | Tstruct typename | Tptr ({desc= Tstruct typename}, _) ->
        Some typename
    | _ ->
        None )


let get_path_classes tenv t =
  let classes =
    match t with
    | Parameter {path= (_, typ), accesses} | Opaque {path= (_, typ), accesses} ->
        classes_of_objects tenv typ accesses
    | Global _ | Class _ ->
        []
  in
  Option.to_list (root_class t) @ classes


let describe fmt t =
  let describe_path fmt path =
    let describe_base fmt (var, _) = Var.pp fmt var in
    pp_with_base describe_base fmt path
  in
  let describe_root fmt t =
    root_class t |> Option.iter ~f:(F.fprintf fmt " in %a" (MF.wrap_monospaced Typ.Name.pp))
  in
  let describe_class_object fmt typename = F.fprintf fmt "%s.class" (Typ.Name.name typename) in
  match t with
  | Class {typename} ->
      MF.wrap_monospaced describe_class_object fmt typename
  | Global {path} | Parameter {path} | Opaque {path} ->
      F.fprintf fmt "%a%a" (MF.wrap_monospaced describe_path) path describe_root t


(* [guarded.(i)] is the lock of the scoped guard passed at position [i], if any *)
type subst = {addresses: t option Array.t; guarded: t option Array.t}

let pp_subst fmt {addresses} =
  PrettyPrintable.pp_collection fmt ~pp_item:(Pp.option pp) (Array.to_list addresses)


(* the address of a local is not opaque: objects on the stack of the caller are not shared *)
let rec make_opaque (hilexp : HilExp.t) =
  match hilexp with
  | AccessExpression access_exp -> (
    match HilExp.AccessExpression.to_accesses access_exp with
    | HilExp.AccessExpression.Base base, accesses
      when not (List.mem accesses MemoryAccess.TakeAddress ~equal:equal_access) ->
        Some (Opaque {path= (base, accesses)})
    | _ ->
        None )
  | Cast (_, hilexp) ->
      make_opaque hilexp
  | _ ->
      None


let make_subst ?(guard_lock = fun _ -> None) formal_map actuals =
  { addresses=
      Array.of_list_map actuals ~f:(fun actual ->
          (* an actual is resolved here as well as at a direct acquisition, so that one mutex does
             not get two different addresses depending on whether it was locked here or passed to
             a callee that locks it *)
          match make formal_map actual |> Option.map ~f:resolve_reference with
          | None ->
              make_opaque actual
          | address ->
              address )
  ; guarded= Array.of_list_map actuals ~f:guard_lock }


let without_opaque subst =
  { subst with
    addresses=
      Array.map subst.addresses ~f:(function Some (Opaque _) -> None | address -> address) }


let apply_subst (subst : subst) t =
  match t with
  | Global _ | Class _ | Opaque _ ->
      Some t
  | Parameter {index; path= (var, _), []} -> (
    try
      (* Special case for when the parameter is used without additional accesses, eg [x] as opposed to [x.f[].g]. *)
      match (subst.guarded.(index), subst.addresses.(index)) with
      | (Some _ as guarded_lock), _ ->
          guarded_lock
      | None, Some (Opaque {path= (_, typ), accesses}) ->
          (* print the parameter rather than the caller's temporary *)
          Some (Opaque {path= ((var, typ), accesses)})
      | None, address ->
          address
    with Invalid_argument _ -> None )
  | Parameter {index; path} -> (
    try
      (* Here we know that there are additional accesses on the parameter *)
      match subst.addresses.(index) with
      | None ->
          None
      | Some (Class _ as t') as c ->
          L.internal_error "Cannot dereference class object %a in path %a@." pp t' pp t ;
          c
      | Some (Parameter param) -> (
        match append ~on_to:param.path path with
        | Some path ->
            Some (Parameter {param with path})
        | None ->
            None )
      | Some (Global global) -> (
        match append ~on_to:global.path path with Some path -> Some (Global {path}) | None -> None )
      | Some (Opaque {path= (_, typ), accesses}) -> (
          let (var, _), _ = path in
          match append ~on_to:((var, typ), accesses) path with
          | Some path ->
              Some (Opaque {path})
          | None ->
              None )
    with Invalid_argument _ -> None )
