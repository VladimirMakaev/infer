(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

open! IStd
module IRAttributes = Attributes
open PulseBasicInterface
open PulseDomainInterface
open PulseOperationResult.Import
open PulseModelsImport
module Collection = PulseModelsGenericArrayBackedCollection
module GenericMapCollection = PulseModelsCpp.GenericMapCollection

(* Models of [std::deque] and of the node-based containers ([std::list], [std::map], [std::set],
   [std::unordered_map], ...) of libc++ and libstdc++, and of their iterators.

   The elements of a container are the cells of its backing array so that [clear()] can invalidate
   all the elements known to the analysis. An iterator stores the address of its element, or the
   past-the-end value of the container, in [node_field], and the backing array of the container in
   [Collection.field]. Insertions do not invalidate the elements of the node-based containers; the
   invalidation of the iterators of an unordered container by rehashing is not modelled.

   [std::deque] iterators also store the current [token_field] of the container: insertions into a
   deque invalidate the token, that is all the iterators, but no element (insertions in the middle of
   a deque also invalidate references, which is not modelled).

   [begin()] and [find()] return the end iterator only when the container is known to be empty so
   that a lookup is not reported for lack of a comparison with [end()]. *)

let node_field = Fieldname.make PulseOperations.pulse_model_type "__infer_node"

let token_field = Fieldname.make PulseOperations.pulse_model_type "__infer_iterators_token"

let desc class_name name = Printf.sprintf "std::%s::%s()" class_name name

let has_token (container : Invalidation.std_container) =
  match container with Deque -> true | _ -> false


let load_field path location obj field astate =
  PulseOperations.eval_deref_access path Read location obj (FieldAccess field) astate


let store_field path location event obj field (value, hist) astate =
  PulseOperations.write_deref_field path location ~ref:obj field
    ~obj:(value, Hist.add_event event hist)
    astate


let ordered_map_cache_field =
  Fieldname.make PulseOperations.pulse_model_type "__infer_ordered_map_cache"


let ordered_map_complete_field =
  Fieldname.make PulseOperations.pulse_model_type "__infer_ordered_map_complete"


(* Keep node identity for later key erasure, but forget membership/cardinality proofs affected by
   an unsupported mutation. Insertion cannot make an already-present key disappear. *)
let forget_ordered_map_cache ?(insertion = false) path location this astate =
  match
    Memory.find_edge_opt (fst this) (FieldAccess ordered_map_cache_field) astate
    |> Option.bind ~f:(fun cell -> Memory.find_edge_opt (fst cell) Dereference astate)
  with
  | None ->
      Ok astate
  | Some cache ->
      let astate, zero = PulseArithmetic.absval_of_int astate IntLit.zero in
      let astate =
        Memory.add_edge path cache (FieldAccess ordered_map_complete_field)
          (zero, snd cache)
          location astate
      in
      Ok
        (Memory.fold_edges (fst cache) astate ~init:astate ~f:(fun astate (access, value) ->
             match access with
             | FieldAccess field
               when String.is_prefix (Fieldname.get_field_name field)
                      ~prefix:"__infer_ordered_map_lookup_valid_" ->
                 Memory.add_edge path cache access (zero, snd value) location astate
             | FieldAccess field
               when String.is_prefix (Fieldname.get_field_name field)
                      ~prefix:"__infer_ordered_map_present_" ->
                 if
                   insertion
                   && Option.exists
                        (PulseArithmetic.as_constant_q astate (fst value))
                        ~f:(fun q -> not (Q.equal q Q.zero))
                 then astate
                 else
                   Memory.add_edge path cache access
                     (AbstractValue.mk_fresh (), snd value)
                     location astate
             | _ ->
                 astate ) )


let end_function = Formula.Procname (Procname.from_string_c_fun "__infer_end_of_collection")

(* The past-the-end value of a container is a function of its backing array rather than a cell of
   it: comparing an element, abduced in the pre, with a value of the pre would be an assumption on
   the heap of the caller that makes all the paths after a loop over a container parameter latent,
   and a cell added in the post only would hide the writes through the elements from impurity. *)
let eval_end path location container astate =
  let=* astate, arr = Collection.eval path Read location container astate in
  let end_value = AbstractValue.mk_fresh () in
  let++ astate =
    PulseArithmetic.and_equal (AbstractValueOperand end_value)
      (FunctionApplicationOperand {f= end_function; actuals= [fst arr]})
      astate
  in
  (AddressAttributes.mark_as_end_of_collection end_value astate, (end_value, snd arr))


let fresh_element path location container astate =
  Collection.element path location container (AbstractValue.mk_fresh ()) astate


let set_size_positive path location event container astate =
  let size = AbstractValue.mk_fresh () in
  let=* astate =
    PulseOperations.write_deref_field path location ~ref:container Collection.size_field
      ~obj:(size, Hist.single_event event)
      astate
  in
  PulseArithmetic.and_positive size astate


let make_iterator ~has_token path location event container iter elem astate =
  let* astate, arr = Collection.eval path Read location container astate in
  let* astate = store_field path location event iter Collection.field arr astate in
  let* astate = store_field path location event iter node_field elem astate in
  if has_token then
    let* astate, token = load_field path location container token_field astate in
    store_field path location event iter token_field token astate
  else Ok astate


let copy_iterator ~has_token path location event ~src ~dst astate =
  let fields = Collection.field :: node_field :: (if has_token then [token_field] else []) in
  PulseResult.list_fold fields ~init:astate ~f:(fun astate field ->
      let* astate, value = load_field path location src field astate in
      store_field path location event dst field value astate )


(** check that [iter] is valid, and that it is not the end iterator unless [allow_end], then return
    its element and the backing array of its container *)
let check_iterator ~has_token ~allow_end path location iter astate =
  let* astate, elem = load_field path location iter node_field astate in
  let* astate =
    if allow_end then Ok astate else Collection.Iterator.check_not_end location elem astate
  in
  let* astate =
    if has_token then
      let* astate, token = load_field path location iter token_field astate in
      PulseOperations.check_addr_access path NoAccess location token astate
    else Ok astate
  in
  let* astate = PulseOperations.check_addr_access path NoAccess location elem astate in
  (* [clear()] invalidates the backing array, hence the iterators obtained before *)
  let* astate, arr = load_field path location iter Collection.field astate in
  let+ astate = PulseOperations.check_addr_access path NoAccess location arr astate in
  (astate, elem, arr)


(* the fields of the element are invalidated too, e.g. the mapped value of a map entry *)
let invalidate_element path location cause elem astate =
  let rec invalidate_fields (visited, astate) addr =
    Memory.fold_edges (fst addr) astate ~init:(visited, astate)
      ~f:(fun (visited, astate) (access, field_addr) ->
        match (access : Access.t) with
        | FieldAccess _ when not (AbstractValue.Set.mem (fst field_addr) visited) ->
            let astate =
              PulseOperations.invalidate path
                (MemoryAccess {pointer= addr; access; hist_obj_default= snd field_addr})
                location cause field_addr astate
            in
            invalidate_fields (AbstractValue.Set.add (fst field_addr) visited, astate) field_addr
        | _ ->
            (visited, astate) )
  in
  let astate = PulseOperations.invalidate path UntraceableAccess location cause elem astate in
  invalidate_fields (AbstractValue.Set.singleton (fst elem), astate) elem |> snd


module Iterator = struct
  let constructor this other ~has_token ~desc : model_no_non_disj =
   fun {path; location} astate ->
    let event = Hist.call_event path location desc in
    let<+> astate = copy_iterator ~has_token path location event ~src:other ~dst:this astate in
    astate


  let operator_assign this other ~has_token ~desc : model_no_non_disj =
   fun {path; location; ret= ret_id, _} astate ->
    let event = Hist.call_event path location desc in
    let<+> astate = copy_iterator ~has_token path location event ~src:other ~dst:this astate in
    PulseOperations.write_id ret_id (fst this, Hist.add_event event (snd this)) astate


  let operator_star iter ~has_token ~desc : model_no_non_disj =
   fun {path; location; ret= ret_id, _} astate ->
    let event = Hist.call_event path location desc in
    let<+> astate, (elem, hist), _ =
      check_iterator ~has_token ~allow_end:false path location iter astate
    in
    PulseOperations.write_id ret_id (elem, Hist.add_event event hist) astate


  let step ~has_token step path location event iter astate =
    let allow_end = match step with `PlusPlus -> false | `MinusMinus -> true in
    let* astate, _, arr = check_iterator ~has_token ~allow_end path location iter astate in
    let* astate, elem =
      Collection.eval_element path location arr (AbstractValue.mk_fresh ()) astate
    in
    store_field path location event iter node_field elem astate


  let operator_step step_ iter ~has_token ~desc : model_no_non_disj =
   fun {path; location; ret= ret_id, _} astate ->
    let event = Hist.call_event path location desc in
    let<+> astate = step ~has_token step_ path location event iter astate in
    PulseOperations.write_id ret_id (fst iter, Hist.add_event event (snd iter)) astate


  (** [it++] and [it--] return a copy of the iterator before the step *)
  let operator_step_postfix step_ iter ret_iter ~has_token ~desc : model_no_non_disj =
   fun {path; location} astate ->
    let event = Hist.call_event path location desc in
    let<+> astate =
      let* astate = copy_iterator ~has_token path location event ~src:iter ~dst:ret_iter astate in
      step ~has_token step_ path location event iter astate
    in
    astate


  let operator_compare comparison ~desc iter_lhs iter_rhs : model_no_non_disj =
   fun ({path; location} as model_data) astate ->
    let<*> astate, (elem_lhs, _) = load_field path location iter_lhs node_field astate in
    let<*> astate, (elem_rhs, _) = load_field path location iter_rhs node_field astate in
    Collection.Iterator.compare_values comparison ~desc elem_lhs elem_rhs model_data astate
end

let begin_or_find this iter container ~desc : model_no_non_disj =
 fun {path; location} astate ->
  let event = Hist.call_event path location desc in
  let<++> astate =
    let=* astate, (size, _) = Collection.to_internal_size_deref path Read location this astate in
    let** astate, elem =
      if PulseArithmetic.is_known_zero astate size then eval_end path location this astate
      else Sat (fresh_element path location this astate)
    in
    Sat (make_iterator ~has_token:(has_token container) path location event this iter elem astate)
  in
  astate


let end_ this iter container ~desc : model_no_non_disj =
 fun {path; location} astate ->
  let event = Hist.call_event path location desc in
  let<++> astate =
    let** astate, end_value = eval_end path location this astate in
    Sat
      (make_iterator ~has_token:(has_token container) path location event this iter end_value astate)
  in
  astate


let erase this pos ret_iter container ~desc : model_no_non_disj =
 fun {path; location} astate ->
  let event = Hist.call_event path location desc in
  let has_token = has_token container in
  let<++> astate =
    let=* astate = forget_ordered_map_cache path location this astate in
    let=* astate, elem, _ = check_iterator ~has_token ~allow_end:false path location pos astate in
    let astate = invalidate_element path location (StdContainer (container, Erase)) elem astate in
    let** astate = Collection.decrease_size path location this ~desc astate in
    let=* astate, (size, _) = Collection.to_internal_size_deref path Read location this astate in
    let** astate, next =
      if PulseArithmetic.is_known_zero astate size then eval_end path location this astate
      else Sat (fresh_element path location this astate)
    in
    Sat (make_iterator ~has_token path location event this ret_iter next astate)
  in
  astate


let clear this container ~desc : model_no_non_disj =
 fun {path; location} astate ->
  let cause = Invalidation.StdContainer (container, Clear) in
  let<++> astate =
    let=* astate, arr = Collection.eval path NoAccess location this astate in
    let astate =
      Memory.fold_edges (fst arr) astate ~init:astate ~f:(fun astate (access, elem) ->
          match (access : Access.t) with
          (* an element equal to [end()] is the past-the-end value, which [end()] keeps returning *)
          | ArrayAccess _ when not (AddressAttributes.is_end_of_collection (fst elem) astate) ->
              invalidate_element path location cause elem astate
          | _ ->
              astate )
      |> PulseOperations.invalidate_deref_access path location cause this Collection.access
    in
    let=* astate =
      PulseOperations.havoc_deref_field path location this Collection.field
        (Hist.single_call path location desc)
        astate
    in
    Collection.assign_size_constant path location this ~constant:IntLit.zero ~desc astate
  in
  astate


(** [count()] and [contains()] return 0 when the container is known to be empty *)
let count ~desc this : model_no_non_disj =
 fun {path; location; ret= ret_id, _} astate ->
  let ret_val = AbstractValue.mk_fresh () in
  let<++> astate =
    let=* astate, (size, _) = Collection.to_internal_size_deref path Read location this astate in
    let astate =
      PulseOperations.write_id ret_id (ret_val, Hist.single_call path location desc) astate
    in
    if PulseArithmetic.is_known_zero astate size then
      PulseArithmetic.and_eq_int ret_val IntLit.zero astate
    else PulseArithmetic.and_nonnegative ret_val astate
  in
  astate


(* [at()], [front()], and [back()] succeed only on non-empty containers; this is assumed rather
   than pruned, since a pruned size coming from the caller would make all the caller's later issues
   latent *)
let assume_non_empty path location this astate =
  let=* astate, (size, _) = Collection.to_internal_size_deref path Read location this astate in
  PulseArithmetic.and_positive size astate


(** [front()] and [back()] *)
let element_reference ~desc this : model_no_non_disj =
 fun {path; location; ret= ret_id, _} astate ->
  let event = Hist.call_event path location desc in
  let<++> astate =
    let** astate = assume_non_empty path location this astate in
    let=+ astate, (elem, hist) = fresh_element path location this astate in
    Ok (PulseOperations.write_id ret_id (elem, Hist.add_event event hist) astate)
  in
  astate


(** [operator[]] and [at()] of maps return a reference to the mapped value of an element;
    [operator[]] inserts an element if needed *)
let mapped_value_reference ~inserts ~desc this : model_no_non_disj =
 fun {path; location; callee_procname; ret= ret_id, _} astate ->
  let event = Hist.call_event path location desc in
  let<++> astate =
    let=* astate, elem = fresh_element path location this astate in
    let=* astate, (value, hist) =
      match Procname.get_class_type_name callee_procname with
      | Some (CppClass {template_spec_info= Template {args= TType key_t :: TType value_t :: _}}) ->
          PulseOperations.eval_access path Read location elem
            (GenericMapCollection.pair_second_access key_t value_t)
            astate
      | _ ->
          Ok (astate, elem)
    in
    let astate =
      PulseOperations.write_id ret_id (value, Hist.add_event event hist) astate
      |> Decompiler.add_call_source value (Call callee_procname) [(this, StdTyp.void)]
    in
    if inserts then set_size_positive path location event this astate
    else assume_non_empty path location this astate
  in
  astate


(* the user-facing iterator classes of the modelled containers of libc++ and libstdc++, in [std] *)
let iterators =
  [ "__deque_iterator"
  ; "__hash_const_iterator"
  ; "__hash_map_const_iterator"
  ; "__hash_map_iterator"
  ; "__list_const_iterator"
  ; "__list_iterator"
  ; "__map_const_iterator"
  ; "__map_iterator"
  ; "__tree_const_iterator"
  ; "_Deque_iterator"
  ; "_List_const_iterator"
  ; "_List_iterator"
  ; "_Rb_tree_const_iterator"
  ; "_Rb_tree_iterator" ]


(* the iterator classes of the unordered containers of libstdc++, in [std::__detail] *)
let detail_iterators = ["_Node_const_iterator"; "_Node_iterator"]

let containers : (string * Invalidation.std_container) list =
  [ ("deque", Deque)
  ; ("list", List)
  ; ("map", Map)
  ; ("multimap", Multimap)
  ; ("multiset", Multiset)
  ; ("set", Set)
  ; ("unordered_map", UnorderedMap)
  ; ("unordered_multimap", UnorderedMultimap)
  ; ("unordered_multiset", UnorderedMultiset)
  ; ("unordered_set", UnorderedSet) ]


(* like the name matchers of [ProcnameDispatcher], ignore the template arguments that the frontend
   appends to some names *)
let strip_template_args name = String.lsplit2 name ~on:'<' |> Option.value_map ~f:fst ~default:name

let is_iterator name =
  let name = strip_template_args name in
  List.mem iterators name ~equal:String.equal || List.mem detail_iterators name ~equal:String.equal


let container_of_name name =
  List.Assoc.find containers ~equal:String.equal (strip_template_args name)


let class_name (name : Typ.Name.t) =
  match name with
  | CppClass {name} ->
      QualifiedCppName.extract_last name
      |> Option.map ~f:(fun (last, _) -> strip_template_args last)
  | _ ->
      None


let callee_class_name callee_procname =
  Procname.get_class_type_name callee_procname |> Option.bind ~f:class_name


(** the iterator returned by a call whose last argument is [ret], directly or as the [first] field
    of a [std::pair] *)
let returned_iterator path location event (ret : _ FuncArg.t option) astate =
  let is_iterator_typ (typ : Typ.t) =
    match typ.desc with
    | Tstruct name ->
        Option.exists (class_name name) ~f:is_iterator
    | _ ->
        false
  in
  match ret with
  | Some {arg_payload= ret; typ= {desc= Tptr (typ, _)}} when is_iterator_typ typ ->
      Some (Ok (astate, ret))
  | Some
      { arg_payload= ret
      ; typ=
          { desc=
              Tptr
                ( { desc=
                      Tstruct
                        (CppClass {template_spec_info= Template {args= TType iter_typ :: _}} as pair)
                  }
                , _ ) } }
    when Option.exists (class_name pair) ~f:(String.equal "pair") && is_iterator_typ iter_typ ->
      Some
        (let* astate =
           PulseOperations.write_deref_field path location ~ref:ret (Fieldname.make pair "second")
             ~obj:(AbstractValue.mk_fresh (), Hist.single_event event)
             astate
         in
         PulseOperations.eval_access path Read location ret
           (FieldAccess (Fieldname.make pair "first"))
           astate )
  | _ ->
      None


(** insertions into a [std::deque] invalidate all its iterators; references to its elements are not
    invalidated, which is only right for insertions at either end *)
let deque_insertion deque_f ~ret ~desc this args : model_no_non_disj =
 fun {path; location; ret= ret_id, _} astate ->
  let event = Hist.call_event path location desc in
  let<++> astate =
    let astate =
      PulseOperations.invalidate_deref_access path location
        (StdContainer (Deque, deque_f))
        this (FieldAccess token_field) astate
    in
    let=* astate =
      PulseOperations.havoc_deref_field path location this token_field (Hist.single_event event)
        astate
    in
    let** astate = set_size_positive path location event this astate in
    match ret with
    | `Reference ->
        let=+ astate, (elem, hist) = fresh_element path location this astate in
        Ok (PulseOperations.write_id ret_id (elem, Hist.add_event event hist) astate)
    | `Iterator -> (
      match returned_iterator path location event (List.last args) astate with
      | Some result ->
          let=* astate, ret_iter = result in
          let=* astate, elem = fresh_element path location this astate in
          Sat (make_iterator ~has_token:true path location event this ret_iter elem astate)
      | None ->
          Sat (Ok astate) )
    | `Nothing ->
        Sat (Ok astate)
  in
  astate


(** A node-based container owns its elements, so destroying it invalidates them. Without this a
    value tied to an element stays reachable for ever, and a local container holding the only owner
    of an allocation no longer looks like a leak. [std::vector] has had this since its [~vector]
    model; this brings the node-based containers to the same footing. *)
let destructor this container ~desc : model_no_non_disj =
 fun {path; location} astate ->
  let cause = Invalidation.StdContainer (container, Destructor) in
  let<+> astate, array = Collection.eval path NoAccess location this astate in
  let addresses =
    AbductiveDomain.reachable_addresses_from
      ~edge_filter:(function Access.FieldAccess _ | ArrayAccess _ -> true | Dereference -> false)
      (Seq.return (fst array))
      astate `Post
  in
  ignore desc ;
  AbstractValue.Set.fold
    (fun addr astate ->
      PulseOperations.invalidate path UntraceableAccess location cause (addr, snd array) astate )
    addresses astate


(** insertions into a node-based container invalidate nothing; one that returns an iterator inserts
    an element, or finds an equal one *)
let node_insertion ~desc this args : model_no_non_disj =
 fun {path; location} astate ->
  let event = Hist.call_event path location desc in
  let<++> astate =
    let=* astate = forget_ordered_map_cache ~insertion:true path location this astate in
    match returned_iterator path location event (List.last args) astate with
    | Some result ->
        let=* astate, ret_iter = result in
        let=* astate, elem = fresh_element path location this astate in
        let=* astate =
          make_iterator ~has_token:false path location event this ret_iter elem astate
        in
        set_size_positive path location event this astate
    | None ->
        let=* astate =
          PulseOperations.havoc_deref_field path location this Collection.size_field
            (Hist.single_event event) astate
        in
        Sat (Ok astate)
  in
  astate


let callee_desc callee_procname class_name =
  desc class_name (strip_template_args (Procname.get_method callee_procname))


let on_iterator model : model_no_non_disj =
 fun ({callee_procname} as model_data) astate ->
  let iterator = callee_class_name callee_procname |> Option.value ~default:"" in
  model
    ~has_token:(List.mem ["__deque_iterator"; "_Deque_iterator"] iterator ~equal:String.equal)
    ~desc:(callee_desc callee_procname iterator)
    model_data astate


let on_container model : model_no_non_disj =
 fun ({callee_procname} as model_data) astate ->
  let class_name = callee_class_name callee_procname |> Option.value ~default:"" in
  match container_of_name class_name with
  | Some container ->
      model container ~desc:(callee_desc callee_procname class_name) model_data astate
  | None ->
      Logging.die InternalError "%a is not a method of a modelled container" Procname.pp
        callee_procname


module OrderedMap = struct
  let complete_field = ordered_map_complete_field

  let key_field prefix key =
    Fieldname.make PulseOperations.pulse_model_type (prefix ^ IntLit.to_string key)


  let node_key key = key_field "__infer_ordered_map_node_" key

  let present_key key = key_field "__infer_ordered_map_present_" key

  let lookup_valid_key key = key_field "__infer_ordered_map_lookup_valid_" key

  let is_std_type last = function
    | {Typ.desc= Tstruct (CppClass {name})} ->
        String.is_prefix (QualifiedCppName.to_qual_string name) ~prefix:"std::"
        && Option.exists (QualifiedCppName.extract_last name) ~f:(fun (name, _) ->
            String.equal (strip_template_args name) last )
    | _ ->
        false


  (* No equivalence of user-defined comparators, or constructor callbacks, is assumed. *)
  let supports callee =
    match Procname.get_class_type_name callee with
    | Some
        (CppClass
           { template_spec_info=
               Template {args= [TType key; TType value; TType compare; TType allocator]} } ) ->
        let compares_keys =
          match compare.Typ.desc with
          | Tstruct (CppClass {template_spec_info= Template {args= [TType compared]}}) ->
              Typ.equal compared key
          | _ ->
              false
        in
        let allocates_values =
          match allocator.Typ.desc with
          | Tstruct (CppClass {template_spec_info= Template {args= [TType pair]}}) -> (
              is_std_type "pair" pair
              &&
              match pair.Typ.desc with
              | Tstruct
                  (CppClass
                     {template_spec_info= Template {args= [TType alloc_key; TType alloc_value]}} )
                ->
                  Typ.equal alloc_key (Typ.set_to_const key) && Typ.equal alloc_value value
              | _ ->
                  false )
          | _ ->
              false
        in
        Typ.is_int key
        && (Typ.is_int value || Typ.is_pointer value)
        && is_std_type "less" compare && compares_keys && is_std_type "allocator" allocator
        && allocates_values
    | _ ->
        false


  let edge astate addr field = Memory.find_edge_opt (fst addr) (FieldAccess field) astate

  let cache this astate =
    edge astate this ordered_map_cache_field
    |> Option.bind ~f:(fun cell -> Memory.find_edge_opt (fst cell) Dereference astate)


  let flag astate cache field =
    edge astate cache field
    |> Option.bind ~f:(fun (v, _) -> PulseArithmetic.as_constant_q astate v)
    |> Option.map ~f:(fun q -> not (Q.equal q Q.zero))


  let write_flag path location cache field flag astate =
    let astate, value =
      PulseArithmetic.absval_of_int astate (IntLit.of_int (if flag then 1 else 0))
    in
    Memory.add_edge path cache (FieldAccess field) (value, snd cache) location astate


  let key_value path location (key : _ FuncArg.t) astate =
    let* astate, (value, _) =
      if Typ.is_pointer key.typ then
        PulseOperations.eval_access path Read location key.arg_payload Dereference astate
      else Ok (astate, key.arg_payload)
    in
    let constant =
      PulseArithmetic.as_constant_q astate value
      |> Option.bind ~f:(fun q ->
          Option.some_if (Z.equal (Q.den q) Z.one) (IntLit.of_big_int (Q.num q)) )
    in
    Ok (astate, constant)


  let is_empty path location this astate =
    let+ astate, (size, _) = Collection.to_internal_size_deref path Read location this astate in
    (astate, PulseArithmetic.is_known_zero astate size)


  let presence path location this key astate =
    let* astate, empty = is_empty path location this astate in
    let known =
      match cache this astate with
      | None ->
          Option.some_if empty false
      | Some cache -> (
        match edge astate cache (node_key key) with
        | Some (node, _)
          when AddressAttributes.is_end_of_collection node astate
               && Option.value (flag astate cache (lookup_valid_key key)) ~default:false ->
            Some false
        | _ -> (
          match flag astate cache (present_key key) with
          | Some _ as known ->
              known
          | None ->
              Option.some_if
                (empty || Option.value (flag astate cache complete_field) ~default:false)
                false ) )
    in
    Ok (astate, known)


  let ensure_cache path location event this astate =
    match cache this astate with
    | Some cache ->
        Ok (astate, cache)
    | None ->
        let* astate, empty = is_empty path location this astate in
        let cache = (AbstractValue.mk_fresh (), Hist.single_event event) in
        (* This is analysis-only lookup state, not an observable write to a user's map. *)
        let astate, cell = Memory.eval_edge this (FieldAccess ordered_map_cache_field) astate in
        let astate =
          AbductiveDomain.set_post_edges (fst cell)
            (UnsafeMemory.Edges.add Dereference cache UnsafeMemory.Edges.empty)
            astate
          |> AddressAttributes.initialize (fst cell)
        in
        Ok (write_flag path location cache complete_field empty astate, cache)


  let existing_node this key astate =
    cache this astate |> Option.bind ~f:(fun c -> edge astate c (node_key key))


  let live_node this key astate =
    existing_node this key astate
    |> Option.filter ~f:(fun (node, _) ->
        (not (AddressAttributes.is_end_of_collection node astate))
        && AddressAttributes.find_opt `Post node astate
           |> Option.bind ~f:Attributes.get_invalid
           |> Option.is_none )


  let value_reference path location callee elem astate =
    match Procname.get_class_type_name callee with
    | Some (CppClass {template_spec_info= Template {args= TType key_t :: TType value_t :: _}}) ->
        PulseOperations.eval_access path Read location elem
          (GenericMapCollection.pair_second_access key_t value_t)
          astate
    | _ ->
        Ok (astate, elem)


  let new_node path location event callee this key ~initialize astate =
    let=* astate, cache = ensure_cache path location event this astate in
    let=* astate, elem = fresh_element path location this astate in
    (* The cache is only lookup evidence. Nodes remain owned by the collection after that
       evidence is forgotten, so clear() still invalidates previously returned references. *)
    let astate = Memory.add_edge path cache (FieldAccess (node_key key)) elem location astate in
    let astate = write_flag path location cache (present_key key) true astate in
    let=* astate, value = value_reference path location callee elem astate in
    let=* astate =
      if initialize then
        let astate, zero = PulseArithmetic.absval_of_int astate IntLit.zero in
        PulseOperations.write_deref path location ~ref:value ~obj:(zero, snd elem) astate
      else Ok astate
    in
    let** astate, endpoint = eval_end path location this astate in
    let++ astate =
      PulseArithmetic.and_not_equal
        (AbstractValueOperand (fst elem))
        (AbstractValueOperand (fst endpoint))
        astate
    in
    (astate, elem)


  let mapped_reference ~inserts this (key : _ FuncArg.t) : model_no_non_disj =
   fun ({path; location; callee_procname; ret= ret_id, _} as model_data) astate ->
    let desc = callee_desc callee_procname "map" in
    let event = Hist.call_event path location desc in
    let<*> astate, constant = key_value path location key astate in
    match constant with
    | None ->
        let<*> astate, unknown_key =
          if Typ.is_pointer key.typ then
            PulseOperations.eval_access path Read location key.arg_payload Dereference astate
          else Ok (astate, key.arg_payload)
        in
        (* Return the actual cached reference when keys match. Acquiring a reference is not a
           write; the caller's subsequent store must update the corresponding cached value. *)
        let targets =
          match cache this astate with
          | None ->
              []
          | Some cache ->
              Memory.fold_edges (fst cache) astate ~init:[] ~f:(fun targets (access, elem) ->
                  match access with
                  | FieldAccess field
                    when String.is_prefix (Fieldname.get_field_name field)
                           ~prefix:"__infer_ordered_map_node_"
                         && (not (AddressAttributes.is_end_of_collection (fst elem) astate))
                         && Option.is_none
                              ( AddressAttributes.find_opt `Post (fst elem) astate
                              |> Option.bind ~f:Attributes.get_invalid ) ->
                      let text =
                        String.chop_prefix_exn (Fieldname.get_field_name field)
                          ~prefix:"__infer_ordered_map_node_"
                      in
                      let constant = IntLit.of_big_int (Z.of_string text) in
                      if Option.value (flag astate cache (present_key constant)) ~default:false then
                        (constant, elem) :: targets
                      else targets
                  | _ ->
                      targets )
        in
        let matches =
          List.concat_map targets ~f:(fun (constant, elem) ->
              let<**> astate =
                PulseArithmetic.and_equal
                  (AbstractValueOperand (fst unknown_key))
                  (ConstOperand (Cint constant)) astate
              in
              let<+> astate, (value, hist) =
                value_reference path location callee_procname elem astate
              in
              PulseOperations.write_id ret_id (value, Hist.add_event event hist) astate
              |> Decompiler.add_call_source value (Call callee_procname) [(this, StdTyp.void)] )
        in
        let fallback =
          let<**> astate =
            List.fold targets ~init:(Sat (Ok astate)) ~f:(fun result (constant, _) ->
                let** astate = result in
                PulseArithmetic.and_not_equal
                  (AbstractValueOperand (fst unknown_key))
                  (ConstOperand (Cint constant)) astate )
          in
          let<*> astate =
            if inserts then forget_ordered_map_cache ~insertion:true path location this astate
            else Ok astate
          in
          mapped_value_reference ~inserts ~desc this model_data astate
        in
        matches @ fallback
    | Some key -> (
        let<*> astate, known = presence path location this key astate in
        let finish known existing astate =
          let<**> astate, elem =
            match existing with
            | Some elem ->
                Sat (Ok (astate, elem))
            | None ->
                new_node path location event callee_procname this key
                  ~initialize:(inserts && Option.equal Bool.equal known (Some false))
                  astate
          in
          let astate =
            match cache this astate with
            | Some cache when inserts ->
                write_flag path location cache (present_key key) true astate
            | _ ->
                astate
          in
          let<**> astate =
            if not inserts then Sat (Ok astate)
            else if Option.equal Bool.equal known (Some false) then
              Collection.increase_size path location this ~desc astate
            else if Option.equal Bool.equal known (Some true) then Sat (Ok astate)
            else set_size_positive path location event this astate
          in
          let<+> astate, (value, hist) =
            value_reference path location callee_procname elem astate
          in
          PulseOperations.write_id ret_id (value, Hist.add_event event hist) astate
          |> Decompiler.add_call_source value (Call callee_procname) [(this, StdTyp.void)]
        in
        match (live_node this key astate, known) with
        | Some probe, None ->
            (* A prior unknown find may have returned end(). Keep the saved iterator's identity
               on the hit path, and create a different, initialized node on the miss path. *)
            let<**> astate, endpoint = eval_end path location this astate in
            let hit =
              let<**> astate =
                PulseArithmetic.and_not_equal
                  (AbstractValueOperand (fst probe))
                  (AbstractValueOperand (fst endpoint))
                  astate
              in
              finish (Some true) (Some probe) astate
            in
            let miss =
              let<**> astate =
                PulseArithmetic.and_equal
                  (AbstractValueOperand (fst probe))
                  (AbstractValueOperand (fst endpoint))
                  astate
              in
              finish (Some false) None astate
            in
            hit @ miss
        | Some elem, Some true ->
            finish known (Some elem) astate
        | _ ->
            finish known None astate )


  let find this key iter : model_no_non_disj =
   fun ({path; location; callee_procname} as model_data) astate ->
    let desc = callee_desc callee_procname "map" in
    let event = Hist.call_event path location desc in
    let<*> astate, constant = key_value path location key astate in
    match constant with
    | None ->
        begin_or_find this iter Map ~desc model_data astate
    | Some key ->
        let<*> astate, known = presence path location this key astate in
        let<**> astate, elem =
          if Option.equal Bool.equal known (Some false) then eval_end path location this astate
          else
            Sat
              ( match live_node this key astate with
              | Some elem ->
                  Ok (astate, elem)
              | None ->
                  let* astate, elem = fresh_element path location this astate in
                  let+ astate, cache = ensure_cache path location event this astate in
                  (* A successful lookup gives a node identity, not an insertion or a proof of
                     closed-world membership/cardinality on an input map. *)
                  ( Memory.add_edge path cache (FieldAccess (node_key key)) elem location astate
                    |> write_flag path location cache (lookup_valid_key key) true
                  , elem ) )
        in
        let<+> astate = make_iterator ~has_token:false path location event this iter elem astate in
        astate


  let erase (this : _ FuncArg.t) (key : _ FuncArg.t) : model_no_non_disj =
   fun ({path; location; callee_procname; ret= ret_id, _} as model_data) astate ->
    let desc = callee_desc callee_procname "map" in
    let<*> astate, constant = key_value path location key astate in
    match constant with
    | None ->
        let<*> astate, empty = is_empty path location this.arg_payload astate in
        if empty then Basic.return_int ~desc 0L model_data astate
        else
          let<*> astate = forget_ordered_map_cache path location this.arg_payload astate in
          Basic.skipped_known_call [this; key] model_data astate
    | Some key ->
        let<*> astate, known = presence path location this.arg_payload key astate in
        let astate =
          match live_node this.arg_payload key astate with
          | Some elem ->
              invalidate_element path location (StdContainer (Map, Erase)) elem astate
          | None ->
              astate
        in
        let astate =
          match cache this.arg_payload astate with
          | Some cache ->
              write_flag path location cache (present_key key) false astate
          | None ->
              astate
        in
        let<**> astate =
          match known with
          | Some true ->
              Collection.decrease_size path location this.arg_payload ~desc astate
          | Some false ->
              Sat (Ok astate)
          | None ->
              Sat
                (PulseOperations.havoc_deref_field path location this.arg_payload
                   Collection.size_field
                   (Hist.single_call path location desc)
                   astate )
        in
        let astate, count =
          match known with
          | Some present ->
              PulseArithmetic.absval_of_int astate (IntLit.of_int (if present then 1 else 0))
          | None ->
              (astate, AbstractValue.mk_fresh ())
        in
        Basic.ok_continue
          (PulseOperations.write_id ret_id (count, Hist.single_call path location desc) astate)


  let clear this : model_no_non_disj =
   fun ({path; location} as model_data) astate ->
    let astate =
      match cache this astate with
      | None ->
          astate
      | Some cache ->
          Memory.fold_edges (fst cache) astate ~init:astate ~f:(fun astate (access, elem) ->
              match access with
              | FieldAccess field
                when String.is_prefix (Fieldname.get_field_name field)
                       ~prefix:"__infer_ordered_map_node_"
                     && not (AddressAttributes.is_end_of_collection (fst elem) astate) ->
                  invalidate_element path location (StdContainer (Map, Clear)) elem astate
              | _ ->
                  astate )
    in
    let<*> astate = forget_ordered_map_cache path location this astate in
    clear this Map ~desc:"std::map::clear()" model_data astate
end

(** a constructor whose body is translated, eg from headers under the project root, runs instead of
    the model: the other translated methods read the fields that it sets. The size is then unknown,
    as these methods do not maintain it. *)
let default_constructor ({FuncArg.arg_payload= this; typ} : _ FuncArg.t) : model =
 fun ({analysis_data; path; location; callee_procname; ret} as model_data) astate non_disj ->
  let is_translated =
    Option.exists (IRAttributes.load callee_procname) ~f:(fun {ProcAttributes.is_defined} ->
        is_defined )
  in
  if is_translated then
    let astates, non_disj, _, _ =
      PulseCallOperations.call analysis_data path location callee_procname ~ret
        ~actuals:[(this, typ)]
        ~formals_opt:None ResolvedCall CallFlags.default astate non_disj
    in
    (astates, non_disj)
  else
    ( on_container (fun _ ~desc -> Collection.default_constructor ~desc this) model_data astate
    , non_disj )


let matchers : matcher list =
  let open ProcnameDispatcher.Call in
  let detail () = -"std" &:: "__detail" in
  let iterator_typs () =
    List.map iterators ~f:(fun iterator -> -"std" &:: iterator)
    @ List.map detail_iterators ~f:(fun iterator -> detail () &:: iterator)
  in
  let is_iterator _ name = is_iterator name in
  let is_container ?(f = fun _ -> true) _ name = Option.exists (container_of_name name) ~f in
  let ordered_map_method name =
    -"std"
    &::+ (fun (_, procname) name ->
    String.equal (strip_template_args name) "map" && OrderedMap.supports procname )
    &:: name
  in
  let ordered_map_matchers =
    [ ordered_map_method "operator[]" <>$ capt_arg_payload $+ capt_arg
      $--> OrderedMap.mapped_reference ~inserts:true
    ; ordered_map_method "find" <>$ capt_arg_payload $+ capt_arg $+ capt_arg_payload
      $--> OrderedMap.find
    ; ordered_map_method "erase" <>$ capt_arg $+ capt_arg $--> OrderedMap.erase
    ; ordered_map_method "clear" <>$ capt_arg_payload $--> OrderedMap.clear ]
  in
  let iterator_matchers ~in_detail =
    let namespace () = if in_detail then detail () else -"std" in
    let iterator_method name = namespace () &::+ is_iterator &:: name in
    [ ( namespace () &::+ is_iterator &::+ is_iterator $ capt_arg_payload
      $+ capt_arg_payload_of_typ_exists (iterator_typs ())
      $--> fun this other -> on_iterator (Iterator.constructor this other) )
    ; ( iterator_method "operator=" <>$ capt_arg_payload $+ capt_arg_payload
      $--> fun this other -> on_iterator (Iterator.operator_assign this other) )
    ; ( iterator_method "operator*" <>$ capt_arg_payload
      $--> fun iter -> on_iterator (Iterator.operator_star iter) )
    ; ( iterator_method "operator->" <>$ capt_arg_payload
      $--> fun iter -> on_iterator (Iterator.operator_star iter) )
    ; ( iterator_method "operator++" <>$ capt_arg_payload
      $--> fun iter -> on_iterator (Iterator.operator_step `PlusPlus iter) )
    ; ( iterator_method "operator++" <>$ capt_arg_payload $+ any_arg $+ capt_arg_payload
      $--> fun iter ret_iter -> on_iterator (Iterator.operator_step_postfix `PlusPlus iter ret_iter)
      )
    ; ( iterator_method "operator--" <>$ capt_arg_payload
      $--> fun iter -> on_iterator (Iterator.operator_step `MinusMinus iter) )
    ; ( iterator_method "operator--" <>$ capt_arg_payload $+ any_arg $+ capt_arg_payload
      $--> fun iter ret_iter ->
      on_iterator (Iterator.operator_step_postfix `MinusMinus iter ret_iter) ) ]
  in
  (* the iterators of the unordered containers of libstdc++ are compared as their base class *)
  let comparison_matchers =
    let compared_typs () = (detail () &:: "_Node_iterator_base") :: iterator_typs () in
    List.concat_map
      [-"std"; detail ()]
      ~f:(fun namespace ->
        [ namespace &:: "operator=="
          $ capt_arg_payload_of_typ_exists (compared_typs ())
          $+ capt_arg_payload_of_typ_exists (compared_typs ())
          $--> Iterator.operator_compare `Equal ~desc:"iterator operator=="
        ; namespace &:: "operator!="
          $ capt_arg_payload_of_typ_exists (compared_typs ())
          $+ capt_arg_payload_of_typ_exists (compared_typs ())
          $--> Iterator.operator_compare `NotEqual ~desc:"iterator operator!=" ] )
  in
  let container_method ?f name = -"std" &::+ is_container ?f &:: name in
  let returns_iterator name model =
    container_method name <>$ capt_arg_payload
    $+ capt_arg_payload_of_typ_exists (iterator_typs ())
    $--> fun this iter -> on_container (model this iter)
  in
  let is_node_based : Invalidation.std_container -> bool = function Deque -> false | _ -> true in
  let container_matchers =
    [ returns_iterator "begin" begin_or_find
    ; returns_iterator "cbegin" begin_or_find
    ; returns_iterator "end" end_
    ; returns_iterator "cend" end_
    ; ( container_method "erase" <>$ capt_arg_payload
      $+ capt_arg_payload_of_typ_exists (iterator_typs ())
      $+ capt_arg_payload
      $--> fun this pos ret_iter -> on_container (erase this pos ret_iter) )
    ; ( container_method ~f:is_node_based "insert"
      $ capt_arg_payload
      $++$--> fun this args -> on_container (fun _ ~desc -> node_insertion ~desc this args) )
    ; (container_method "clear" <>$ capt_arg_payload $--> fun this -> on_container (clear this))
    ; ( container_method "empty" <>$ capt_arg_payload
      $--> fun this -> on_container (fun _ ~desc -> Collection.empty ~desc this) )
    ; ( container_method "size" <>$ capt_arg_payload
      $--> fun this -> on_container (fun _ ~desc -> Collection.size ~desc this) ) ]
  in
  let is_associative : Invalidation.std_container -> bool = function
    | Deque | List ->
        false
    | Map
    | Multimap
    | Multiset
    | Set
    | UnorderedMap
    | UnorderedMultimap
    | UnorderedMultiset
    | UnorderedSet ->
        true
  in
  let lookup_matchers =
    (* [lower_bound] and [upper_bound] return an iterator to an element of the container, as
       [find] does, so they take the same model. Without one the returned iterator is unrelated
       to the container, and a value stored through it is reachable from nothing. *)
    List.map ["find"; "lower_bound"; "upper_bound"] ~f:(fun method_name ->
        container_method ~f:is_associative method_name
        $ capt_arg_payload $+ any_arg
        $+ capt_arg_payload_of_typ_exists (iterator_typs ())
        $--> fun this iter -> on_container (begin_or_find this iter) )
    @ [ ( container_method ~f:is_associative "count"
        $ capt_arg_payload $+ any_arg
        $--> fun this -> on_container (fun _ ~desc -> count ~desc this) )
      ; ( container_method ~f:is_associative "contains"
        $ capt_arg_payload $+ any_arg
        $--> fun this -> on_container (fun _ ~desc -> count ~desc this) ) ]
  in
  let is_sequence container = not (is_associative container) in
  let front_back_matchers =
    [ ( container_method ~f:is_sequence "front"
      <>$ capt_arg_payload
      $--> fun this -> on_container (fun _ ~desc -> element_reference ~desc this) )
    ; ( container_method ~f:is_sequence "back"
      <>$ capt_arg_payload
      $--> fun this -> on_container (fun _ ~desc -> element_reference ~desc this) ) ]
  in
  (* the adapters keep their elements in an underlying container and have no iterators of their
     own, so they are not in [containers]; their element accessors still have to return a
     reference into the adapter, or a pointer stored through one is unreachable *)
  let adapter_matchers =
    List.concat_map
      [("stack", ["top"]); ("queue", ["front"; "back"]); ("priority_queue", ["top"])]
      ~f:(fun (adapter, accessors) ->
        List.map accessors ~f:(fun accessor ->
            -"std" &:: adapter &:: accessor <>$ capt_arg_payload
            $--> fun this -> element_reference ~desc:(desc adapter accessor) this ) )
  in
  let is_map : Invalidation.std_container -> bool = function
    | Map | UnorderedMap ->
        true
    | _ ->
        false
  in
  let map_matchers =
    [ ( container_method ~f:is_map "operator[]"
      <>$ capt_arg_payload $+ any_arg
      $--> fun this -> on_container (fun _ ~desc -> mapped_value_reference ~inserts:true ~desc this)
      )
    ; ( container_method ~f:is_map "at" <>$ capt_arg_payload $+ any_arg
      $--> fun this ->
      on_container (fun _ ~desc -> mapped_value_reference ~inserts:false ~desc this) ) ]
  in
  let deque_desc = desc "deque" in
  let deque_matchers =
    [ -"std" &:: "deque" &:: "operator[]" <>$ capt_arg_payload $+ capt_arg_payload
      $--> PulseModelsCpp.Vector.at ~desc:(deque_desc "operator[]")
    ; -"std" &:: "deque" &:: "at" <>$ capt_arg_payload $+ capt_arg_payload
      $--> PulseModelsCpp.Vector.at ~desc:(deque_desc "at")
    ; -"std" &:: "deque" &:: "push_back" <>$ capt_arg_payload
      $++$--> deque_insertion PushBack ~ret:`Nothing ~desc:(deque_desc "push_back")
    ; -"std" &:: "deque" &:: "push_front" <>$ capt_arg_payload
      $++$--> deque_insertion PushFront ~ret:`Nothing ~desc:(deque_desc "push_front")
    ; -"std" &:: "deque" &:: "emplace_back" $ capt_arg_payload
      $++$--> deque_insertion EmplaceBack ~ret:`Reference ~desc:(deque_desc "emplace_back")
    ; -"std" &:: "deque" &:: "emplace_front" $ capt_arg_payload
      $++$--> deque_insertion EmplaceFront ~ret:`Reference ~desc:(deque_desc "emplace_front")
    ; -"std" &:: "deque" &:: "insert" $ capt_arg_payload
      $++$--> deque_insertion Insert ~ret:`Iterator ~desc:(deque_desc "insert")
    ; -"std" &:: "deque" &:: "emplace" $ capt_arg_payload
      $++$--> deque_insertion Emplace ~ret:`Iterator ~desc:(deque_desc "emplace") ]
  in
  let destructor_matchers =
    List.map containers ~f:(fun (container, _) ->
        -"std" &:: container &:: "~" ^ container <>$ capt_arg_payload
        $--> fun this -> on_container (destructor this) )
  in
  ordered_map_matchers
  @ iterator_matchers ~in_detail:false
  @ iterator_matchers ~in_detail:true @ comparison_matchers @ container_matchers @ lookup_matchers
  @ front_back_matchers @ map_matchers @ deque_matchers @ adapter_matchers @ destructor_matchers
  |> List.map ~f:(ProcnameDispatcher.Call.map_matcher ~f:lift_model)
  |> List.cons (-"std" &::+ is_container &::+ is_container <>$ capt_arg $--> default_constructor)
  |> List.map ~f:(ProcnameDispatcher.Call.contramap_arg_payload ~f:ValueOrigin.addr_hist)
