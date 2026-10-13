(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

open! IStd
module IRAttributes = Attributes
module L = Logging
open PulseBasicInterface
open PulseDomainInterface
open PulseOperationResult.Import
open PulseModelsImport
module GenericArrayBackedCollection = PulseModelsGenericArrayBackedCollection

(* NOTE: The semantic models do not check overflow for now. *)
let binop_overflow_common binop (x, x_hist) (y, y_hist) res : model_no_non_disj =
 fun {path; location} astate ->
  let bop_addr = AbstractValue.mk_fresh () in
  let<**> astate, bop_addr = PulseArithmetic.eval_binop_absval bop_addr binop x y astate in
  let hist = Hist.binop binop x_hist y_hist in
  let<+> astate = PulseOperations.write_deref path location ~ref:res ~obj:(bop_addr, hist) astate in
  astate


let add_overflow = binop_overflow_common (PlusA None)

let mul_overflow = binop_overflow_common (Mult None)

let sub_overflow = binop_overflow_common (MinusA None)

let delete deleted_arg : model =
 fun model_data astate -> Basic.free_or_delete `Delete CppDelete deleted_arg model_data astate


(* NOTE: [new\[\]] is not yet modelled as allocating an array of objects hence why this model
   deletes only the root address *)
let delete_array deleted_arg : model =
 fun model_data astate -> Basic.free_or_delete `Delete CppDeleteArray deleted_arg model_data astate


let new_ type_exp =
  let open PulseModelsDSL.Syntax in
  start_named_model "new" @@ fun () -> new_ type_exp >>= assign_ret


(* TODO: actually allocate an array  *)
let new_array type_name : model_no_non_disj =
 fun model_data astate ->
  let<++> astate =
    (* Java, Hack and C++ [new\[\]] share the same builtin *)
    let pdesc = Procdesc.get_proc_name model_data.analysis_data.proc_desc in
    if Procname.is_java pdesc || Procname.is_hack pdesc || Procname.is_python pdesc then
      Basic.alloc_no_leak_not_null ~initialize:true (Some type_name) ~desc:"new[]" model_data astate
    else
      (* C++ *)
      Basic.alloc_not_null ~initialize:true ~desc:"new[]" CppNewArray (Some type_name) model_data
        astate
  in
  astate


(** [actuals] are the size of the object followed by the placement arguments of the new-expression.
    The frontend does not call the [operator new] that the new-expression selects, so the object is
    only known to be constructed in place when exactly one placement argument is a [void*], as in
    [new (buf) T]. [cf_return_null_checked] is set when the allocation function may return null. *)
let placement_new =
  let std_nothrow_t_matcher = QualifiedCppName.Match.of_fuzzy_qual_names ["std::nothrow_t"] in
  let is_nothrow_t {FuncArg.typ} =
    match typ.Typ.desc with
    | Tstruct (CppClass {name}) ->
        QualifiedCppName.Match.match_qualifiers std_nothrow_t_matcher name
    | _ ->
        false
  in
  let placement_new_no_alloc ~null_checked actuals : model_no_non_disj =
   fun {path; location; ret= ret_id, _} astate ->
    let event = Hist.call_event path location "<placement new>()" in
    match List.filter actuals ~f:(fun {FuncArg.typ} -> Typ.is_pointer_to_void typ) with
    | [{FuncArg.arg_payload= address, hist}] ->
        let astate = PulseOperations.write_id ret_id (address, Hist.add_event event hist) astate in
        (* Under a null check, an unknown storage is assumed not to be null, or the initialization
           would be skipped. Without one, constraining a storage parameter would make callers that
           pass null infeasible instead of reporting the dereference in the constructor. *)
        if null_checked && not (PulseArithmetic.is_known_zero astate address) then
          let<++> astate = PulseArithmetic.and_positive address astate in
          astate
        else Basic.ok_continue astate
    | _ when null_checked && List.exists actuals ~f:is_nothrow_t ->
        (* a non-throwing allocation that may return null, e.g. with an explicit alignment *)
        PulseOperations.havoc_id ret_id (Hist.single_event event) astate |> Basic.ok_continue
    | _ ->
        (* assume that the allocation function succeeds *)
        let ret_addr = AbstractValue.mk_fresh () in
        let<++> astate = PulseArithmetic.and_positive ret_addr astate in
        PulseOperations.write_id ret_id (ret_addr, Hist.single_event event) astate
  in
  fun actuals ({call_flags} as model_data) astate ->
    let null_checked = call_flags.CallFlags.cf_return_null_checked in
    match (actuals : _ FuncArg.t list) with
    | [{exp= size_exp}; nothrow] when null_checked && is_nothrow_t nothrow ->
        let allocator, desc =
          match (size_exp : Exp.t) with
          | BinOp (Mult _, Sizeof _, _) ->
              (Attribute.CppNewArray, "new[](std::nothrow)")
          | _ ->
              (Attribute.CppNew, "new(std::nothrow)")
        in
        PulseModelsC.alloc_common ~null_case:(not Config.pulse_unsafe_malloc) ~initialize:true ~desc
          allocator (Some size_exp) model_data astate
    | _ ->
        lift_model (placement_new_no_alloc ~null_checked actuals) model_data astate


let infer_structured_binding var {FuncArg.exp= arg; arg_payload} _ astate =
  let astate =
    match (var, arg) with
    | Exp.Lvar pvar, Var arg ->
        AbductiveDomain.Stack.remove_vars [Var.of_id arg] astate
        |> AbductiveDomain.Stack.add (Var.of_pvar pvar) (ValueOrigin.unknown arg_payload)
    | _ ->
        L.internal_error "Unexpected arguments for c17_structured_binding: %a, %a@\n" Exp.pp var
          Exp.pp arg ;
        astate
  in
  Basic.ok_continue astate


module AtomicInteger = struct
  let internal_int =
    Fieldname.make
      (Typ.CStruct (QualifiedCppName.of_list ["std"; "atomic"]))
      "__infer_model_backing_int"


  let load_backing_int path location this astate =
    let* astate, obj = PulseOperations.eval_access path Read location this Dereference astate in
    let* astate, int_addr =
      PulseOperations.eval_access path Read location obj (FieldAccess internal_int) astate
    in
    let+ astate, int_val =
      PulseOperations.eval_access path Read location int_addr Dereference astate
    in
    (astate, int_addr, int_val)


  let constructor this_address init_value : model_no_non_disj =
   fun {path; location} astate ->
    let this =
      (AbstractValue.mk_fresh (), Hist.single_call path location "std::atomic::atomic()")
    in
    let<*> astate, int_field =
      PulseOperations.eval_access path Write location this (FieldAccess internal_int) astate
    in
    let<*> astate =
      PulseOperations.write_deref path location ~ref:int_field ~obj:init_value astate
    in
    let<+> astate = PulseOperations.write_deref path location ~ref:this_address ~obj:this astate in
    astate


  let arith_bop path prepost location event ret_id bop this operand astate =
    let=* astate, int_addr, (old_int, old_hist) = load_backing_int path location this astate in
    let hist = Hist.add_event event old_hist in
    let bop_addr = AbstractValue.mk_fresh () in
    let+* astate, bop_addr =
      PulseArithmetic.eval_binop bop_addr bop (AbstractValueOperand old_int) operand astate
    in
    let+ astate =
      PulseOperations.write_deref path location ~ref:int_addr ~obj:(bop_addr, hist) astate
    in
    let ret_int = match prepost with `Pre -> bop_addr | `Post -> old_int in
    PulseOperations.write_id ret_id (ret_int, hist) astate


  let fetch_add this (increment, _) _memory_ordering : model_no_non_disj =
   fun {path; location; ret= ret_id, _} astate ->
    let event = Hist.call_event path location "std::atomic::fetch_add()" in
    let<++> astate =
      arith_bop path `Post location event ret_id (PlusA None) this (AbstractValueOperand increment)
        astate
    in
    astate


  let fetch_sub this (increment, _) _memory_ordering : model_no_non_disj =
   fun {path; location; ret= ret_id, _} astate ->
    let event = Hist.call_event path location "std::atomic::fetch_sub()" in
    let<++> astate =
      arith_bop path `Post location event ret_id (MinusA None) this (AbstractValueOperand increment)
        astate
    in
    astate


  let operator_plus_plus_pre this : model_no_non_disj =
   fun {path; location; ret= ret_id, _} astate ->
    let event = Hist.call_event path location "std::atomic::operator++()" in
    let<++> astate =
      arith_bop path `Pre location event ret_id (PlusA None) this (ConstOperand (Cint IntLit.one))
        astate
    in
    astate


  let operator_plus_plus_post this _int : model_no_non_disj =
   fun {path; location; ret= ret_id, _} astate ->
    let event = Hist.call_event path location "std::atomic<T>::operator++(T)" in
    let<++> astate =
      arith_bop path `Post location event ret_id (PlusA None) this (ConstOperand (Cint IntLit.one))
        astate
    in
    astate


  let operator_minus_minus_pre this : model_no_non_disj =
   fun {path; location; ret= ret_id, _} astate ->
    let event = Hist.call_event path location "std::atomic::operator--()" in
    let<++> astate =
      arith_bop path `Pre location event ret_id (MinusA None) this (ConstOperand (Cint IntLit.one))
        astate
    in
    astate


  let operator_minus_minus_post this _int : model_no_non_disj =
   fun {path; location; ret= ret_id, _} astate ->
    let event = Hist.call_event path location "std::atomic<T>::operator--(T)" in
    let<++> astate =
      arith_bop path `Post location event ret_id (MinusA None) this (ConstOperand (Cint IntLit.one))
        astate
    in
    astate


  let load_instr model_desc this _memory_ordering_opt : model_no_non_disj =
   fun {path; location; ret= ret_id, _} astate ->
    let<+> astate, _int_addr, (int, hist) = load_backing_int path location this astate in
    PulseOperations.write_id ret_id (int, Hist.add_call path location model_desc hist) astate


  let load this memory_ordering_opt = load_instr "std::atomic<T>::load()" this memory_ordering_opt

  let operator_t this memory_ordering_opt =
    load_instr "std::atomic<T>::operator_T()" this memory_ordering_opt


  let store_backing_int ({PathContext.timestamp} as path) location this_address new_value astate =
    let* astate, this =
      PulseOperations.eval_access path Read location this_address Dereference astate
    in
    let astate =
      AddressAttributes.add_one (fst this_address)
        (WrittenTo (timestamp, Trace.Immediate {location; history= ValueHistory.epoch}))
        astate
    in
    let* astate, int_field =
      PulseOperations.eval_access path Write location this (FieldAccess internal_int) astate
    in
    PulseOperations.write_deref path location ~ref:int_field ~obj:new_value astate


  let store this_address (new_value, new_hist) _memory_ordering : model_no_non_disj =
   fun {path; location} astate ->
    let<+> astate =
      store_backing_int path location this_address
        (new_value, Hist.add_call path location "std::atomic::store()" new_hist)
        astate
    in
    astate


  let exchange this_address (new_value, new_hist) _memory_ordering : model_no_non_disj =
   fun {path; location; ret= ret_id, _} astate ->
    let event = Hist.call_event path location "std::atomic::exchange()" in
    let<*> astate, _int_addr, (old_int, old_hist) =
      load_backing_int path location this_address astate
    in
    let<+> astate =
      store_backing_int path location this_address (new_value, Hist.add_event event new_hist) astate
    in
    PulseOperations.write_id ret_id (old_int, Hist.add_event event old_hist) astate
end

(** the address of the element at [index] of the buffer [data], kept reachable from [data] so that
    invalidating the buffer invalidates the derived pointers too *)
let buffer_offset ~desc data index : PulseModelsDSL.aval PulseModelsDSL.model_monad =
  let open PulseModelsDSL.Syntax in
  let* {path; location} = get_data in
  exec_partial_operation (fun astate ->
      let astate, element = Memory.eval_edge data (ArrayAccess (StdTyp.void, fst index)) astate in
      let++ astate, address =
        PulseArithmetic.eval_binop (fst element) PlusPI
          (AbstractValueOperand (fst data))
          (AbstractValueOperand (fst index))
          astate
      in
      let element = (address, Hist.add_call path location desc (snd data)) in
      let astate =
        AddressAttributes.find_opt `Post (fst data) astate
        |> Option.bind ~f:Attributes.get_invalid
        |> Option.value_map ~default:astate ~f:(fun (cause, trace) ->
            AddressAttributes.add_one address (Invalid (cause, trace)) astate )
      in
      ( Memory.add_edge path data (ArrayAccess (StdTyp.void, fst index)) element location astate
      , element ) )


module BasicString = struct
  (* Derived data pointers stay reachable from the buffer cell, without sharing the character
     contents of independently owned strings. *)
  let invalidate_offsets path location cause buffer astate =
    let rec visit seen buffer astate =
      Memory.fold_edges (fst buffer) astate ~init:(seen, astate)
        ~f:(fun (seen, astate) (access, child) ->
          match access with
          | ArrayAccess _ when not (AbstractValue.Set.mem (fst child) seen) ->
              let seen = AbstractValue.Set.add (fst child) seen in
              let astate =
                PulseOperations.invalidate path
                  (MemoryAccess {pointer= buffer; access; hist_obj_default= snd child})
                  location cause child astate
              in
              visit seen child astate
          | _ ->
              (seen, astate) )
    in
    visit (AbstractValue.Set.singleton (fst buffer)) buffer astate |> snd


  let offset data index = buffer_offset ~desc:"string buffer offset" data index

  let constructor_from_constant_dsl (this, this_hist) init_hist : unit PulseModelsDSL.model_monad =
    let open PulseModelsDSL.Syntax in
    let* this_hist' = add_model_call this_hist in
    store_field ~ref:(this, this_hist') ModeledField.internal_string init_hist
    @@>
    let* s_opt = as_constant_string init_hist in
    let* len =
      exec_operation (fun astate ->
          match s_opt with
          | Some s ->
              let astate, len =
                String.length s |> IntLit.of_int |> PulseArithmetic.absval_of_int astate
              in
              (len, astate)
          | None ->
              (AbstractValue.mk_fresh_restricted (), astate) )
    in
    write_field ~ref:(this, this_hist') ModeledField.string_length (len, snd init_hist)


  (** constructor from constant string *)
  let constructor_from_constant ~desc this_hist init_hist : model =
    let open PulseModelsDSL.Syntax in
    start_named_model desc @@ fun () -> constructor_from_constant_dsl this_hist init_hist


  let default_constructor ~desc this_hist : model =
    let open PulseModelsDSL.Syntax in
    start_named_model desc
    @@ fun () ->
    let* init_hist = string "" in
    constructor_from_constant_dsl this_hist init_hist


  let fill_constructor ~desc this_hist length : model =
    let open PulseModelsDSL.Syntax in
    start_named_model desc
    @@ fun () ->
    let* contents = fresh () in
    store_field ~ref:this_hist ModeledField.internal_string contents
    @@> write_field ~ref:this_hist ModeledField.string_length length


  let copy_constructor_dsl (this, hist) src_hist : unit PulseModelsDSL.model_monad =
    let open PulseModelsDSL.Syntax in
    let* this_hist = add_model_call hist in
    let* src_string = load_access src_hist (FieldAccess ModeledField.internal_string) in
    let* src_length = access Read src_hist (FieldAccess ModeledField.string_length) in
    store_field ~ref:(this, this_hist) ModeledField.internal_string src_string
    @@> write_field ~ref:(this, this_hist) ModeledField.string_length src_length


  let copy_constructor ~desc this_hist src_hist : model =
    let open PulseModelsDSL.Syntax in
    start_named_model desc @@ fun () -> copy_constructor_dsl this_hist src_hist


  let data ((this, hist) as this_hist) ~desc : model =
    let open PulseModelsDSL.Syntax in
    start_named_model desc
    @@ fun () ->
    let* this_string, this_string_hist =
      access Read this_hist (FieldAccess ModeledField.internal_string)
    in
    let* ret_hist = add_model_call this_string_hist in
    exec_command
      (AbductiveDomain.AddressAttributes.add_one this_string
         (PropagateTaintFrom (InternalModel, [{v= this; history= hist}])) )
    @@> assign_ret (this_string, ret_hist)


  let iterator_common ((this, hist) as this_hist) ((iter, _) as iter_hist) :
      (PulseModelsDSL.aval * PulseModelsDSL.aval) PulseModelsDSL.model_monad =
    let open PulseModelsDSL.Syntax in
    let* backing_ptr = access Read iter_hist (FieldAccess GenericArrayBackedCollection.field) in
    let* internal_string =
      load_access ~deref:false this_hist (FieldAccess ModeledField.internal_string)
    in
    exec_command
      (AbductiveDomain.AddressAttributes.add_one iter
         (PropagateTaintFrom (InternalModel, [{v= this; history= hist}])) )
    @@> ret (backing_ptr, internal_string)


  let begin_ this_hist iter_hist ~desc : model =
    let open PulseModelsDSL.Syntax in
    start_named_model desc
    @@ fun () ->
    let* backing_ptr, (string, hist) = iterator_common this_hist iter_hist in
    let* hist' = add_model_call hist in
    store ~ref:backing_ptr (string, hist')


  let end_ this_hist iter_hist ~desc : model =
    let open PulseModelsDSL.Syntax in
    start_named_model desc
    @@ fun () ->
    let* {path; location} = get_data in
    let* backing_ptr, string_hist = iterator_common this_hist iter_hist in
    let* last, hist =
      exec_partial_operation (fun astate ->
          Sat
            (GenericArrayBackedCollection.eval_pointer_to_last_element path location string_hist
               astate ) )
    in
    let* hist' = add_model_call hist in
    store ~ref:backing_ptr (last, hist')


  let destructor this_hist : model =
    let open PulseModelsDSL.Syntax in
    start_named_model "std::basic_string::~basic_string()"
    @@ fun () ->
    let* {path; location} = get_data in
    let call_event = Hist.call_event path location "std::basic_string::~basic_string()" in
    let* string_addr, string_hist =
      access Read this_hist (FieldAccess ModeledField.internal_string)
    in
    let string_hist = Hist.add_event call_event string_hist in
    exec_partial_command (fun astate ->
        let astate = invalidate_offsets path location CppDelete (string_addr, string_hist) astate in
        Sat
          (PulseOperations.check_and_invalidate path
             (MemoryAccess
                { pointer= this_hist
                ; access= FieldAccess ModeledField.internal_string
                ; hist_obj_default= string_hist } )
             location CppDelete (string_addr, string_hist) astate ) )


  (** Mutating member functions may reallocate the buffer, so the buffer returned by [data()] or
      [c_str()] before the call is invalidated and replaced by a fresh one. The call is otherwise
      treated as unknown, also for taint (see [PulseTaintOperations]), except that the overloads
      returning [basic_string&] return the receiver. *)
  let mutator string_f ({FuncArg.arg_payload= this} as this_arg) args : model_no_non_disj =
   fun {path; callee_procname; analysis_data= {tenv}; location; ret} astate ->
    let internal_string = PulseOperations.ModeledField.internal_string in
    let new_buffer =
      let desc = Format.asprintf "%a()" Invalidation.pp_std_string_function string_f in
      (AbstractValue.mk_fresh (), Hist.single_call path location desc)
    in
    let<*> astate =
      let* astate, buffer =
        PulseOperations.eval_access path NoAccess location this (FieldAccess internal_string) astate
      in
      let astate = invalidate_offsets path location (StdString string_f) buffer astate in
      PulseOperations.invalidate_access path location (StdString string_f) this
        (FieldAccess internal_string) astate
      |> PulseOperations.write_field path location ~ref:this internal_string ~obj:new_buffer
    in
    let actuals =
      List.map (this_arg :: args) ~f:(fun {FuncArg.arg_payload; typ} -> (arg_payload, typ))
    in
    let attrs_opt = IRAttributes.load callee_procname in
    let formals_opt = Option.map attrs_opt ~f:ProcAttributes.get_pvar_formals in
    let returns_this =
      match attrs_opt with
      | Some {ProcAttributes.ret_type= {desc= Tptr ({desc= Tstruct ret_class}, _)}} ->
          Option.exists (Procname.get_class_type_name callee_procname) ~f:(Typ.Name.equal ret_class)
      | _ ->
          false
    in
    let<++> astate =
      PulseCallOperations.unknown_call tenv path location (SkippedKnownCall callee_procname)
        (Some callee_procname) ~ret ~actuals ~formals_opt astate
    in
    if returns_this then PulseOperations.write_id (fst ret) this astate else astate


  let empty this_hist : model =
    let open PulseModelsDSL.Syntax in
    start_named_model "std::basic_string::empty()"
    @@ fun () ->
    let* len = access Read this_hist (FieldAccess ModeledField.string_length) in
    let* string = load_access this_hist (FieldAccess ModeledField.internal_string) in
    let* ret_hist = add_model_call (snd string) in
    disj
      [ prune_eq_zero len @@> prune_eq_string string "" @@> assign_ret @= int ~hist:ret_hist 1
      ; prune_positive len @@> prune_ne_string string "" @@> assign_ret @= int ~hist:ret_hist 0 ]


  let length this_hist : model =
    let open PulseModelsDSL.Syntax in
    start_named_model "std::basic_string::length()"
    @@ fun () ->
    let* {path; location} = get_data in
    let event = Hist.call_event path location "std::basic_string::length()" in
    let* length, hist = access Read this_hist (FieldAccess ModeledField.string_length) in
    assign_ret (length, Hist.add_event event hist)


  let size this : model =
   fun ({path; location; ret= ret_id, _} as model_data) astate non_disj ->
    let known_length =
      Memory.find_edge_opt (fst this) (FieldAccess PulseOperations.ModeledField.string_length)
        astate
      |> Option.exists ~f:(fun (length, _) ->
          Option.is_some (PulseArithmetic.as_constant_q astate length) )
    in
    if known_length then
      let ( let<*> ) x f = bind_sat_result non_disj (Sat x) f in
      let<*> astate, (length, hist) =
        PulseOperations.eval_access path Read location this
          (FieldAccess PulseOperations.ModeledField.string_length) astate
      in
      ( Basic.ok_continue
          (PulseOperations.write_id ret_id
             (length, Hist.add_call path location "std::basic_string::size()" hist)
             astate )
      , non_disj )
    else lift_model (Basic.nondet ~desc:"std::basic_string::size") model_data astate non_disj


  let address ~desc ptr_hist (idx, _) : model_no_non_disj =
   fun ({ret= ret_id, _} as model_env) astate ->
    let astate_zero_idx =
      let++ astate = PulseArithmetic.prune_eq_zero idx astate in
      PulseOperations.write_id ret_id ptr_hist astate |> Basic.continue
    in
    let astate_non_zero_idx =
      let<**> astate = PulseArithmetic.prune_ne_zero idx astate in
      Basic.nondet ~desc model_env astate
    in
    SatUnsat.to_list astate_zero_idx @ astate_non_zero_idx
end

(** A [std::basic_string_view] keeps the string value and length of what it views like [BasicString]
    does, and in [data_field] the pointer returned by [data()]: the viewed [char*] or the buffer of
    the viewed [std::basic_string]. Copies of a view share that pointer, so they see the
    invalidation of the viewed buffer. *)
module BasicStringView = struct
  let supports callee =
    match Procname.get_class_type_name callee with
    | Some (CppClass {template_spec_info= Template {args= [TType char; TType traits]}})
      when Typ.equal char StdTyp.char -> (
      match traits.Typ.desc with
      | Tstruct (CppClass {name; template_spec_info= Template {args= [TType trait_char]}}) ->
          String.is_prefix (QualifiedCppName.to_qual_string name) ~prefix:"std::"
          && Option.exists (QualifiedCppName.extract_last name) ~f:(fun (last, _) ->
              String.equal
                (String.lsplit2 last ~on:'<' |> Option.value_map ~default:last ~f:fst)
                "char_traits" )
          && Typ.equal trait_char char
      | _ ->
          false )
    | _ ->
        false


  let data_field = Fieldname.make PulseOperations.pulse_model_type "__infer_model_string_view_data"

  let lifetime_field =
    Fieldname.make PulseOperations.pulse_model_type "__infer_model_string_view_lifetime"


  let store_data this_hist (data, data_hist) : unit PulseModelsDSL.model_monad =
    let open PulseModelsDSL.Syntax in
    let* data_hist = add_model_call data_hist in
    store_field ~ref:this_hist data_field (data, data_hist)
    @@> store_field ~ref:this_hist lifetime_field (data, data_hist)


  let default_constructor ~desc this_hist : model =
    let open PulseModelsDSL.Syntax in
    start_named_model desc
    @@ fun () ->
    let* zero = int 0 in
    write_field ~ref:this_hist ModeledField.string_length zero @@> store_data this_hist zero


  let constructor_from_char_ptr ~desc this_hist ptr_hist : model =
    let open PulseModelsDSL.Syntax in
    start_named_model desc
    @@ fun () ->
    BasicString.constructor_from_constant_dsl this_hist ptr_hist @@> store_data this_hist ptr_hist


  (** [basic_string_view(const char* s, size_t count)] and the C++20 iterator-pair constructor *)
  let constructor_from_char_ptr_and_size ~desc ((this, hist) as this_hist) ptr_hist
      (size_arg : PulseModelsDSL.aval FuncArg.t) : model =
    let open PulseModelsDSL.Syntax in
    start_named_model desc
    @@ fun () ->
    store_data this_hist ptr_hist
    @@>
    if Typ.is_int size_arg.typ then
      let* hist = add_model_call hist in
      write_field ~ref:(this, hist) ModeledField.string_length size_arg.arg_payload
    else ret ()


  let copy_dsl this_hist src_hist : unit PulseModelsDSL.model_monad =
    let open PulseModelsDSL.Syntax in
    BasicString.copy_constructor_dsl this_hist src_hist
    @@> let* data = load_access src_hist (FieldAccess data_field) in
        store_data this_hist data
        @@> let* lifetime = load_access src_hist (FieldAccess lifetime_field) in
            store_field ~ref:this_hist lifetime_field lifetime


  let copy_constructor ~desc this_hist src_hist : model =
    let open PulseModelsDSL.Syntax in
    start_named_model desc @@ fun () -> copy_dsl this_hist src_hist


  let assign ~desc this_hist src_hist : model =
    let open PulseModelsDSL.Syntax in
    start_named_model desc @@ fun () -> copy_dsl this_hist src_hist @@> assign_ret this_hist


  (** [std::basic_string::operator basic_string_view()] *)
  let of_string ~desc string_hist this_hist : model =
    let open PulseModelsDSL.Syntax in
    start_named_model desc
    @@ fun () ->
    BasicString.copy_constructor_dsl this_hist string_hist
    @@> let* buffer =
          load_access ~deref:false string_hist (FieldAccess ModeledField.internal_string)
        in
        store_data this_hist buffer


  let data this_hist ~desc : model =
    let open PulseModelsDSL.Syntax in
    start_named_model desc
    @@ fun () ->
    let* data, data_hist = load_access this_hist (FieldAccess data_field) in
    let* hist = add_model_call data_hist in
    assign_ret (data, hist)


  let length ~desc this_hist : model =
    let open PulseModelsDSL.Syntax in
    start_named_model desc
    @@ fun () ->
    let* length = access Read this_hist (FieldAccess ModeledField.string_length) in
    assign_ret length


  let empty this_hist : model =
    let open PulseModelsDSL.Syntax in
    start_named_model "std::basic_string_view::empty()"
    @@ fun () ->
    let* length = access Read this_hist (FieldAccess ModeledField.string_length) in
    disj
      [prune_eq_zero length @@> assign_ret @= int 1; prune_positive length @@> assign_ret @= int 0]


  let address this_hist index : model =
    let open PulseModelsDSL.Syntax in
    start_named_model "std::basic_string_view::operator[]()"
    @@ fun () ->
    let* lifetime = load_access this_hist (FieldAccess lifetime_field) in
    check_valid (ValueOrigin.unknown lifetime)
    @@> let* data = load_access this_hist (FieldAccess data_field) in
        let* element = BasicString.offset data index in
        assign_ret element


  let substr this_hist pos count result : model =
    let open PulseModelsDSL.Syntax in
    start_named_model "std::basic_string_view::substr()"
    @@ fun () ->
    let* length = access Read this_hist (FieldAccess ModeledField.string_length) in
    disj
      [ prune_gt pos length @@> throw
      ; ( prune_le pos length
        @@> let* remaining = binop (MinusA None) length pos in
            let* data = load_access this_hist (FieldAccess data_field) in
            let* lifetime = load_access this_hist (FieldAccess lifetime_field) in
            let* data = BasicString.offset data pos in
            store_data result data
            @@> store_field ~ref:result lifetime_field lifetime
            @@> disj
                  [ prune_le count remaining
                    @@> write_field ~ref:result ModeledField.string_length count
                  ; prune_gt count remaining
                    @@> write_field ~ref:result ModeledField.string_length remaining ] ) ]


  (** Whether [request] is known to be past [limit]. As for [std::span], only constants are
      compared: assuming a symbolic length is exceeded would report on the many correct programs
      whose extent Pulse cannot see, and would do so in the procedure itself, making the issue
      manifest rather than latent. *)
  let known_past ~request ~limit : bool PulseModelsDSL.model_monad =
    let open PulseModelsDSL.Syntax in
    let* request = as_constant_q request in
    let* limit = as_constant_q limit in
    match (request, limit) with
    | Some request, Some limit ->
        ret (Q.compare request limit > 0)
    | _ ->
        ret false


  (** The view now starts or ends outside the sequence it was viewing, so reading through it is
      reported. [size()] is unsigned, so it has wrapped around to a huge value as well. *)
  let out_of_bounds this_hist : unit PulseModelsDSL.model_monad =
    let open PulseModelsDSL.Syntax in
    let* {path; location} = get_data in
    let* data = fresh () in
    exec_command
      (PulseOperations.invalidate path UntraceableAccess location (OutOfBounds StringView) data)
    @@> let* size = fresh_nonneg () in
        store_data this_hist data @@> write_field ~ref:this_hist ModeledField.string_length size


  (** [std::basic_string_view::remove_prefix(n)] and [remove_suffix(n)]. Both require [n] to be at
      most [size()], and neither is checked nor throws. Removing exactly [size()] is allowed and
      leaves an empty view, so only strictly more than it is reported. *)
  let remove_affix ~desc ~moves_data this_hist n : model =
    let open PulseModelsDSL.Syntax in
    start_named_model desc
    @@ fun () ->
    let* length = access Read this_hist (FieldAccess ModeledField.string_length) in
    let* past_the_end = known_past ~request:n ~limit:length in
    if past_the_end then out_of_bounds this_hist
    else
      let* remaining = binop (MinusA None) length n in
      write_field ~ref:this_hist ModeledField.string_length remaining
      @@>
      if moves_data then
        (* the sequence being viewed is the same one, so keep the lifetime it was borrowed with *)
        let* lifetime = load_access this_hist (FieldAccess lifetime_field) in
        let* data = load_access this_hist (FieldAccess data_field) in
        let* data = BasicString.offset data n in
        store_data this_hist data @@> store_field ~ref:this_hist lifetime_field lifetime
      else ret ()
end

module Function = struct
  let call_operator_of_closure tenv lambda astate =
    match PulseArithmetic.get_dynamic_type lambda astate with
    | Some {typ= {desc= Typ.Tstruct name}} -> (
      match Tenv.lookup tenv name with
      | Some tstruct ->
          List.find_map tstruct.Struct.methods ~f:(fun ({name} : Struct.tenv_method) ->
              Option.some_if (Procname.is_cpp_call_operator name) name )
      | None ->
          None )
    | _ ->
        None


  let operator_call ~deref_lambda_ptr FuncArg.{arg_payload= lambda_ptr_hist; typ} actuals : model =
   fun { path
       ; analysis_data
       ; location
       ; callee_procname
       ; ret= (ret_id, _) as ret
       ; dispatch_call_eval_args } astate non_disj ->
    let ( let<*> ) x f = bind_sat_result non_disj (Sat x) f in
    let<*> astate, (lambda, _) =
      (if deref_lambda_ptr then PulseOperations.eval_deref_access else PulseOperations.eval_access)
        path Read location lambda_ptr_hist Dereference astate
    in
    let<*> astate = PulseOperations.Closures.check_captured_addresses path location lambda astate in
    let callee_proc_name_opt =
      match PulseArithmetic.get_dynamic_type lambda astate with
      | Some {typ= {desc= Typ.Tstruct (CFunction csig)}} ->
          Some (`CFunction (Procname.C csig))
      | _ ->
          call_operator_of_closure analysis_data.tenv lambda astate
          |> Option.map ~f:(fun name -> `CallOperator name)
    in
    match callee_proc_name_opt with
    | Some (`CFunction callee_proc_name) ->
        let actuals = List.map actuals ~f:(FuncArg.map_payload ~f:ValueOrigin.unknown) in
        dispatch_call_eval_args analysis_data path ret (Const (Cfun callee_proc_name)) actuals
          location CallFlags.default astate non_disj (Some callee_proc_name)
    | Some (`CallOperator callee_proc_name) ->
        let actuals =
          (lambda_ptr_hist, typ)
          :: List.map actuals ~f:(fun FuncArg.{arg_payload; typ} -> (arg_payload, typ))
        in
        let astate, non_disj, _, _ =
          PulseCallOperations.call analysis_data path location callee_proc_name ~ret ~actuals
            ~formals_opt:None ResolvedCall CallFlags.default astate non_disj
        in
        (astate, non_disj)
    | _ ->
        (* we don't know what proc name this lambda resolves to *)
        let desc = "std::function::operator()" in
        let hist = Hist.single_event (Hist.call_event path location desc) in
        let astate = PulseOperations.havoc_id ret_id hist astate in
        let astate = AbductiveDomain.add_need_dynamic_type_specialization lambda astate in
        let astate =
          (* the types of the actuals lose the qualifiers that implicit conversions add *)
          let formal_types =
            match IRAttributes.load_formal_types callee_procname with
            | _this :: formal_types when Int.equal (List.length formal_types) (List.length actuals)
              ->
                formal_types
            | _ ->
                List.map actuals ~f:(fun {FuncArg.typ} -> typ)
          in
          PulseModelsC.apply_unknown_callee_effect_on_actuals ~desc hist
            (List.map2_exn actuals formal_types ~f:(fun FuncArg.{arg_payload= actual, _} typ ->
                 (actual, typ) ) )
            astate
        in
        ([Ok (ContinueProgram astate)], non_disj)


  (* As for [pthread_once], the callable is assumed to run: if it does not run now then it ran
     during an earlier call with the same flag. *)
  let call_once flag (FuncArg.{arg_payload= callable; typ} as callable_arg) : model =
   fun ({path; analysis_data; location; ret} as model_data) astate non_disj ->
    let call callee_proc_name actuals astate =
      let astate, non_disj, _, _ =
        PulseCallOperations.call analysis_data path location callee_proc_name ~ret ~actuals
          ~formals_opt:None ResolvedCall CallFlags.default astate non_disj
      in
      (astate, non_disj)
    in
    let c_function v astate =
      match PulseArithmetic.get_dynamic_type v astate with
      | Some {typ= {desc= Tstruct (CFunction csig)}} ->
          Some (Procname.C csig)
      | _ ->
          None
    in
    match c_function (fst callable) astate with
    | Some callee_proc_name ->
        call callee_proc_name [] astate
    | None -> (
        let ( let<*> ) x f = bind_sat_result non_disj (Sat x) f in
        let<*> astate, (callee, _) =
          PulseOperations.eval_access path Read location callable Dereference astate
        in
        (* a function pointer is passed by reference *)
        match c_function callee astate with
        | Some callee_proc_name ->
            call callee_proc_name [] astate
        | None -> (
          match call_operator_of_closure analysis_data.tenv callee astate with
          | Some callee_proc_name ->
              call callee_proc_name [(callable, typ)] astate
          | None ->
              lift_model (Basic.skipped_known_call [flag; callable_arg]) model_data astate non_disj
          ) )


  let assign dest FuncArg.{arg_payload= src; typ= src_typ} ~desc : model_no_non_disj =
   fun {path; location; ret= ret_id, _} astate ->
    let event = Hist.call_event path location desc in
    let write_target astate target =
      let<+> astate = PulseOperations.write_deref path location ~ref:dest ~obj:target astate in
      PulseOperations.havoc_id ret_id (Hist.single_event event) astate
    in
    let function_ptr_src =
      match src_typ.Typ.desc with
      | Tptr ({desc= Tfun _}, _) ->
          (* also a reference to a function: the address of the function *)
          Some `Value
      | Tptr ({desc= Tptr ({desc= Tfun _}, _)}, (Pk_lvalue_reference | Pk_rvalue_reference)) ->
          Some `Reference
      | _ ->
          None
    in
    let write_empty_target astate =
      write_target astate (AbstractValue.mk_fresh (), Hist.single_event event)
    in
    match function_ptr_src with
    | Some function_ptr_src ->
        (* the function pointer is not dereferenced, and keeps its dynamic type to resolve calls *)
        let<*> astate, function_ptr =
          match function_ptr_src with
          | `Value ->
              Ok (astate, src)
          | `Reference ->
              PulseOperations.eval_access path Read location src Dereference astate
        in
        if PulseArithmetic.is_known_zero astate (fst function_ptr) then write_empty_target astate
        else write_target astate (fst function_ptr, Hist.add_event event (snd function_ptr))
    | None when PulseArithmetic.is_known_zero astate (fst src) ->
        write_empty_target astate
    | None -> (
      (* with ask_specialization:true we make sure we will copy the dynamic type of the closure object *)
      (* TODO: why do we realloc a closure here? Looks useless since closure are immuable values *)
      match src_typ.Typ.desc with
      | Tptr (_, (Pk_lvalue_reference | Pk_rvalue_reference)) ->
          Basic.shallow_copy ~ask_specialization:true path location event ret_id dest src astate
      | _ ->
          Basic.shallow_copy_value ~ask_specialization:true path location event ret_id dest src
            astate )
end

module ConditionVariable = struct
  let is_this_captured_by_ref field =
    Fieldname.is_capture_field_in_closure_by_ref field
    && String.equal (Fieldname.get_field_name field) (Mangled.to_string Mangled.this)


  (** While [wait] has released the lock, other threads can change anything that the predicate can
      reach. Each field of the predicate's closure (or of the function object) is a cell that holds
      a captured value: we havoc what the captured values point to but keep the cells, which belong
      to the predicate. A captured object stays allocated, as the predicate still reads it, unless a
      captured object points to it, directly or not: that object may own it, and the havoc drops the
      reference. [this] is captured by reference, as the address of the [this] variable, which
      cannot be re-bound: we start from the object it points to and keep the variable. *)
  let havoc_predicate_state ~desc hist pred astate =
    let closure = Memory.find_edge_opt pred Dereference astate |> Option.map ~f:fst in
    let kept, roots =
      List.fold (pred :: Option.to_list closure) ~init:(AbstractValue.Set.empty, [])
        ~f:(fun acc obj ->
          Memory.fold_edges obj astate ~init:acc
            ~f:(fun ((kept, roots) as acc) (access, (cell, _)) ->
              match (access, Memory.find_edge_opt cell Dereference astate) with
              | FieldAccess field, Some (this_var, _) when is_this_captured_by_ref field ->
                  let roots =
                    Memory.find_edge_opt this_var Dereference astate
                    |> Option.fold ~init:roots ~f:(fun roots (this, _) -> this :: roots)
                  in
                  (AbstractValue.Set.add cell kept |> AbstractValue.Set.add this_var, roots)
              | FieldAccess _, Some (captured, _) ->
                  (AbstractValue.Set.add cell kept, captured :: roots)
              | _ ->
                  acc ) )
    in
    let havoc_filter addr _ _ = not (AbstractValue.Set.mem addr kept) in
    let unknown_effect = Attribute.UnknownEffect (Model desc, hist) in
    let owned =
      let successors =
        List.concat_map roots ~f:(fun root ->
            Memory.fold_edges root astate ~init:[] ~f:(fun succs (_, (succ, _)) -> succ :: succs) )
      in
      AbductiveDomain.reachable_addresses_from (Stdlib.List.to_seq successors) astate `Post
    in
    let allocations =
      List.filter_map roots ~f:(fun root ->
          if AbstractValue.Set.mem root owned then None
          else
            AddressAttributes.get_allocation_attr root astate
            |> Option.map ~f:(fun (allocator, trace) ->
                (root, Attribute.Allocated (allocator, trace)) ) )
    in
    let astate =
      List.fold roots ~init:astate ~f:(fun astate root ->
          AbductiveDomain.apply_unknown_effect ~havoc_filter hist root astate
          |> AddressAttributes.add_one root unknown_effect )
    in
    List.fold allocations ~init:astate ~f:(fun astate (root, allocated) ->
        AddressAttributes.add_one root allocated astate )


  (** [operator()] of the predicate: closures (and [std::function]s holding one) are resolved from
      their dynamic type, other function objects from their static type *)
  let resolve_call_operator tenv pred (typ : Typ.t) astate =
    let open IOption.Let_syntax in
    let dynamic_type =
      let* closure, _ = Memory.find_edge_opt pred Dereference astate in
      PulseArithmetic.get_dynamic_type closure astate
    in
    let* class_name =
      match (dynamic_type, typ.desc) with
      | Some {typ= {desc= Tstruct name}}, _ | None, (Tstruct name | Tptr ({desc= Tstruct name}, _))
        ->
          Some name
      | _ ->
          None
    in
    let* {Struct.methods} = Tenv.lookup tenv class_name in
    List.find_map methods ~f:(fun {Struct.name} ->
        Option.some_if (String.equal (Procname.get_method name) "operator()") name )


  (** the results of [pred()], or [None] if we do not know what [pred] does *)
  let call_predicate {FuncArg.arg_payload= pred; typ}
      ({analysis_data= {tenv} as analysis_data; path; location; ret} : model_data) astate non_disj =
    let open IOption.Let_syntax in
    let* callee = resolve_call_operator tenv (fst pred) typ astate in
    match
      PulseCallOperations.call analysis_data path location callee ~ret
        ~actuals:[(pred, typ)]
        ~formals_opt:None ResolvedCall CallFlags.default astate non_disj
    with
    | (_ :: _ as results), non_disj, _, `KnownCall ->
        Some (results, non_disj)
    | _ ->
        (* the callee is unknown or none of its specs applies *)
        None


  (** split the results of [pred()] into the states where it returned true and those where it
      returned false; results that do not continue the program are returned as they are *)
  let split_on_predicate ret_id results =
    List.fold results ~init:([], [], []) ~f:(fun (holds, fails, stopped) result ->
        let split astate errors =
          let astate, (pred_value, _) = PulseOperations.eval_ident ret_id astate in
          let prune f =
            f pred_value astate |> SatUnsat.to_list
            |> List.map ~f:(PulseResult.append_errors errors)
          in
          ( prune PulseArithmetic.prune_ne_zero @ holds
          , prune PulseArithmetic.prune_eq_zero @ fails
          , stopped )
        in
        match (result : ExecutionDomain.t AccessResult.t) with
        | Ok (ContinueProgram astate) ->
            split astate []
        | Recoverable (ContinueProgram astate, errors) ->
            split astate errors
        | _ ->
            (holds, fails, result :: stopped) )


  (** [wait(lock, pred)] is [while (!pred()) wait(lock);]. We unroll the loop once: where [pred()]
      is false, other threads may change what it reads while the lock is released, so we havoc that
      state and assume that [pred()] holds afterwards. The other overloads return the last value of
      [pred()] instead, as they can stop waiting (on timeout or stop request) at any point. *)
  let wait_with_predicate ~desc ~returns_pred_value lock pred : model =
   fun ({path; location; ret= ret_id, _} as model_data) astate non_disj ->
    let hist = Hist.single_call path location desc in
    (* releasing and re-acquiring the lock writes to it, even though its state does not change *)
    let astate =
      AddressAttributes.add_one (fst lock)
        (WrittenTo (path.PathContext.timestamp, Trace.Immediate {location; history= hist}))
        astate
    in
    let continue = List.map ~f:Basic.map_continue in
    let outcomes holds fails = continue holds @ if returns_pred_value then continue fails else [] in
    let wake_up astate non_disj =
      let astate = havoc_predicate_state ~desc hist (fst pred.FuncArg.arg_payload) astate in
      let unknown_result = Ok (ContinueProgram (PulseOperations.havoc_id ret_id hist astate)) in
      match call_predicate pred model_data astate non_disj with
      | None ->
          ([unknown_result], non_disj)
      | Some (results, non_disj) ->
          let holds, fails, stopped = split_on_predicate ret_id results in
          if List.is_empty holds then
            (* [pred()] only depends on state that we do not havoc, e.g. globals *)
            (unknown_result :: stopped, non_disj)
          else (outcomes holds fails @ stopped, non_disj)
    in
    match call_predicate pred model_data astate non_disj with
    | None ->
        wake_up astate non_disj
    | Some (results, non_disj) ->
        let holds, fails, stopped = split_on_predicate ret_id results in
        let woken_up, non_disj =
          NonDisjDomain.bind (fails, non_disj) ~f:(fun fail non_disj ->
              bind_sat_result non_disj (Sat fail) (fun astate -> wake_up astate non_disj) )
        in
        (outcomes holds fails @ woken_up @ stopped, non_disj)
end

module Std = struct
  let make_move_iterator vector : model_no_non_disj =
   fun {path; location; ret= ret_id, _} astate ->
    let<+> astate, (backing_array, _) =
      PulseOperations.eval_deref_access path NoAccess location vector
        (FieldAccess GenericArrayBackedCollection.field) astate
    in
    let astate = AddressAttributes.add_one backing_array StdMoved astate in
    PulseOperations.write_id ret_id
      (fst vector, Hist.add_call path location "std::make_move_iterator" (snd vector))
      astate
end

(** [std::distance], [std::next], [std::prev] and [std::advance] on raw pointers are pointer
    arithmetic; the iterator classes of the containers have their own models *)
module PointerIterator = struct
  let is_function_on_pointers names (_tenv, (proc_name : Procname.t)) name =
    (* [name] carries the template arguments, e.g. [distance<int_*>] *)
    let name = String.lsplit2 name ~on:'<' |> Option.value_map ~default:name ~f:fst in
    List.mem names name ~equal:String.equal
    &&
    match proc_name with
    | C {c_template_args= Template {args= TType typ :: _}} ->
        Typ.is_pointer typ
    | _ ->
        false


  let distance ~desc first last : model =
    let open PulseModelsDSL.Syntax in
    start_named_model desc
    @@ fun () ->
    let* n = binop MinusPP last first in
    assign_ret n


  let offset ~desc op iter (n : PulseModelsDSL.aval option) : model =
    let open PulseModelsDSL.Syntax in
    start_named_model desc
    @@ fun () ->
    let* res = match n with Some n -> binop op iter n | None -> binop_int op iter IntLit.one in
    assign_ret res


  let advance ~desc iter_ref n : model =
    let open PulseModelsDSL.Syntax in
    start_named_model desc
    @@ fun () ->
    let* iter = load iter_ref in
    let* res = binop PlusPI iter n in
    store ~ref:iter_ref res
end

let rec struct_name_of_type (typ : Typ.t) =
  match typ.desc with
  | Tstruct name ->
      Some name
  | Tptr (typ, _) ->
      struct_name_of_type typ
  | _ ->
      None


module ForwardIterator = struct
  let layouts =
    [ ( QualifiedCppName.Match.of_fuzzy_qual_names ["folly::f14::detail::VectorContainerIterator"]
      , [["current_"]; ["lowest_"]] )
    ; ( QualifiedCppName.Match.of_fuzzy_qual_names
          ["folly::f14::detail::ValueContainerIterator"; "folly::f14::detail::NodeContainerIterator"]
      , [["underlying_"; "itemPtr_"]; ["underlying_"; "index_"]] ) ]


  let copy_source_class procname =
    match List.nth (IRAttributes.load_formal_types procname) 1 with
    | Some {Typ.desc= Tptr ({desc= Tstruct name}, (Pk_lvalue_reference | Pk_rvalue_reference))} ->
        Some name
    | _ ->
        None


  let is_copy_method procname =
    match (Procname.get_class_type_name procname, copy_source_class procname) with
    | Some destination, Some source ->
        QualifiedCppName.equal (Typ.Name.qual_name destination) (Typ.Name.qual_name source)
    | _ ->
        false


  let field_paths tenv name =
    let open IOption.Let_syntax in
    let* _, paths =
      List.find layouts ~f:(fun (matcher, _) ->
          QualifiedCppName.Match.match_qualifiers matcher (Typ.Name.qual_name name) )
    in
    let rec resolve name = function
      | [] ->
          Some []
      | field_name :: rest ->
          let* {Struct.fields} = Tenv.lookup tenv name in
          let* {Struct.name= field; typ} =
            List.find fields ~f:(fun ({Struct.name} : Struct.field) ->
                String.equal (Fieldname.get_field_name name) field_name )
          in
          let+ rest =
            match (rest, typ.Typ.desc) with
            | [], Tptr _ when not (String.equal field_name "index_") ->
                Some []
            | [], Tint _ when String.equal field_name "index_" ->
                Some []
            | _ :: _, Tstruct name ->
                resolve name rest
            | _ ->
                None
          in
          field :: rest
    in
    Option.all (List.map paths ~f:(resolve name))


  let rec field_ref path location obj fields astate =
    match fields with
    | [field] ->
        Ok (astate, obj, field)
    | field :: rest ->
        let* astate, obj =
          PulseOperations.eval_access path NoAccess location obj (FieldAccess field) astate
        in
        field_ref path location obj rest astate
    | [] ->
        assert false


  let load_field path location obj fields astate =
    let* astate, obj, field = field_ref path location obj fields astate in
    PulseOperations.eval_deref_access path Read location obj (FieldAccess field) astate


  let copy_position ~src ~dst : model_no_non_disj =
   fun {analysis_data= {tenv}; path; location; callee_procname} astate ->
    let source_paths = Option.bind (copy_source_class callee_procname) ~f:(field_paths tenv) in
    let destination_paths =
      Option.bind (Procname.get_class_type_name callee_procname) ~f:(field_paths tenv)
    in
    let paths =
      match (source_paths, destination_paths) with
      | Some source, Some destination ->
          List.zip_exn source destination
      | _ ->
          []
    in
    let rec copy astate = function
      | [] ->
          Ok astate
      | (source_fields, destination_fields) :: rest ->
          let* astate, value = load_field path location src source_fields astate in
          let* astate, obj, field = field_ref path location dst destination_fields astate in
          let* astate =
            PulseOperations.write_deref_field path location ~ref:obj field ~obj:value astate
          in
          copy astate rest
    in
    let<+> astate = copy astate paths in
    astate


  let distance ~desc (first : _ FuncArg.t) (last : _ FuncArg.t) : model_no_non_disj =
   fun ({analysis_data= {tenv}; path; location; ret= ret_id, _} as model_data) astate ->
    let position typ =
      Option.bind (struct_name_of_type typ) ~f:(field_paths tenv)
      |> Option.bind ~f:(fun paths -> List.hd paths)
    in
    match (position first.typ, position last.typ) with
    | Some first_fields, Some last_fields ->
        let<*> astate, (first_pos, _) =
          load_field path location first.arg_payload first_fields astate
        in
        let<*> astate, (last_pos, _) =
          load_field path location last.arg_payload last_fields astate
        in
        (* These forward iterators compare their positions. For a valid forward range, equality
           means distance zero and inequality means a positive count, not an arbitrary value.
           In particular, do not subtract F14's pointers: iteration can cross noncontiguous chunks. *)
        let result = AbstractValue.mk_fresh () in
        let astate =
          PulseOperations.write_id ret_id (result, Hist.single_call path location desc) astate
        in
        let constrain comparison =
          PulseArithmetic.prune_binop ~negated:false comparison (AbstractValueOperand first_pos)
            (AbstractValueOperand last_pos)
        in
        let empty =
          PulseArithmetic.prune_eq_zero result astate
          >>== constrain Eq >>|| ExecutionDomain.continue
        in
        let nonempty =
          PulseArithmetic.prune_positive result astate
          >>== constrain Ne >>|| ExecutionDomain.continue
        in
        SatUnsat.to_list empty @ SatUnsat.to_list nonempty
    | _ ->
        Basic.nondet ~desc model_data astate
end

(** The frontend translates [{a, b, c}] into [__infer_initializer_list(&arr)]. The return value
    carries the list's begin/size fields; stores and constructors copy those fields onto the actual
    list object, whose begin pointer still refers to the shared backing array. *)
module InitializerList = struct
  let begin_field = Fieldname.make PulseOperations.pulse_model_type "__infer_init_list_begin"

  let size_field = Fieldname.make PulseOperations.pulse_model_type "__infer_init_list_size"

  let is_type =
    let dispatch : (unit, bool, unit) ProcnameDispatcher.TypName.dispatcher =
      let open ProcnameDispatcher.TypName in
      make_dispatcher [-"std" &:: "initializer_list" &--> true]
    in
    fun typ ->
      match typ.Typ.desc with
      | Tstruct (CppClass _ as name) ->
          Option.value (dispatch () name) ~default:false
      | _ ->
          false


  let copy_value path location ~src ~dst astate =
    let* astate, begin_value =
      PulseOperations.eval_deref_access path Read location src (FieldAccess begin_field) astate
    in
    let* astate, size_value =
      PulseOperations.eval_deref_access path Read location src (FieldAccess size_field) astate
    in
    let* astate =
      PulseOperations.write_deref_field path location ~ref:dst begin_field ~obj:begin_value astate
    in
    PulseOperations.write_deref_field path location ~ref:dst size_field ~obj:size_value astate


  let copy_constructor this source ~desc : model_no_non_disj =
   fun {path; location; ret= ret_id, _} astate ->
    let<+> astate = copy_value path location ~src:source ~dst:this astate in
    PulseOperations.write_id ret_id (fst this, Hist.add_call path location desc (snd this)) astate


  let default_constructor this ~desc : model =
    let open PulseModelsDSL.Syntax in
    start_named_model desc
    @@ fun () ->
    let* first = null in
    let* zero = int 0 in
    store_field ~ref:this begin_field first @@> store_field ~ref:this size_field zero


  let make ({FuncArg.arg_payload= arr; typ} : PulseModelsDSL.aval FuncArg.t) : model =
    let open PulseModelsDSL.Syntax in
    start_named_model "infer_init_list"
    @@ fun () ->
    let* hist = add_model_call (snd arr) in
    let list = (fst arr, hist) in
    let length =
      match typ.Typ.desc with
      | Tarray {length= Some n} | Tptr ({desc= Tarray {length= Some n}}, _) ->
          IntLit.to_int n
      | _ ->
          None
    in
    let* () = store_field ~ref:list begin_field list in
    option_iter length ~f:(fun n ->
        let* size = int n in
        store_field ~ref:list size_field size )
    @@> assign_ret list


  let load_field this field =
    let open PulseModelsDSL.Syntax in
    load_access this (FieldAccess field)


  let size ~desc this : model =
    let open PulseModelsDSL.Syntax in
    start_named_model desc @@ fun () -> load_field this size_field >>= assign_ret


  let begin_ ~desc this : model =
    let open PulseModelsDSL.Syntax in
    start_named_model desc @@ fun () -> load_field this begin_field >>= assign_ret


  let end_ ~desc this : model =
    let open PulseModelsDSL.Syntax in
    start_named_model desc
    @@ fun () ->
    let* first = load_field this begin_field in
    let* size = load_field this size_field in
    binop PlusPI first size >>= assign_ret
end

module Vector = struct
  (* This is a lower bound, not an exact capacity. Only concrete bounds are retained: symbolic
     growth keeps the existing reserved-vector approximation without adding summary preconditions. *)
  let capacity_lower_bound vector astate =
    AddressAttributes.find_opt `Post (fst vector) astate
    |> Option.bind ~f:Attributes.get_std_vector_capacity
    |> Option.join
    |> Option.map ~f:(fun capacity -> Q.of_bigint (IntLit.to_big_int capacity))


  let record_capacity vector capacity astate =
    let capacity =
      Option.bind capacity ~f:(fun capacity ->
          if Z.equal (Q.den capacity) Z.one then Some (IntLit.of_big_int (Q.num capacity)) else None )
    in
    AddressAttributes.add_one (fst vector) (StdVectorCapacity capacity) astate


  let post_size field value astate =
    Memory.find_edge_opt (fst value) (FieldAccess field) astate
    |> Option.bind ~f:(fun (pointer, _) -> Memory.find_edge_opt pointer Dereference astate)


  let tracked_positions vector astate =
    Option.is_some (capacity_lower_bound vector astate)
    && Option.exists (post_size GenericArrayBackedCollection.size_field vector astate)
         ~f:(fun (size, _) ->
           Option.exists (PulseArithmetic.as_constant_q astate size) ~f:(fun size ->
               Q.compare size Q.zero > 0 ) )


  let default_constructor vector ~desc : model_no_non_disj =
   fun {path; location} astate ->
    let<**> astate =
      GenericArrayBackedCollection.assign_size_constant path location vector ~constant:IntLit.zero
        ~desc astate
    in
    record_capacity vector (Some Q.zero) astate |> Basic.ok_continue


  let invalidate_element path location cause element astate =
    let addresses =
      AbductiveDomain.reachable_addresses_from
        ~edge_filter:(function FieldAccess _ | ArrayAccess _ -> true | Dereference -> false)
        (Seq.return (fst element))
        astate `Post
    in
    AbstractValue.Set.fold
      (fun addr astate ->
        PulseOperations.invalidate path UntraceableAccess location cause (addr, snd element) astate )
      addresses astate


  let destroy_storage vector : model_no_non_disj =
   fun {path; location} astate ->
    let astate = record_capacity vector None astate in
    let<+> astate, array = GenericArrayBackedCollection.eval path NoAccess location vector astate in
    invalidate_element path location (StdVector Destructor) array astate


  let element_type callee_procname =
    match Procname.get_class_type_name callee_procname with
    | Some (CppClass {template_spec_info= Template {args= TType typ :: _}}) ->
        Some typ
    | _ ->
        None


  let is_smart_pointer =
    let open ProcnameDispatcher.TypName in
    let dispatch =
      make_dispatcher [-"std" &:: "unique_ptr" &::.*--> (); -"std" &:: "shared_ptr" &::.*--> ()]
    in
    fun name -> Option.is_some (dispatch () name)


  let has_pointer_members tenv class_name =
    let rec visit visited name =
      if is_smart_pointer name then true
      else if Typ.Name.Set.mem name visited || String.is_prefix (Typ.Name.name name) ~prefix:"std::"
      then false
      else
        let visited = Typ.Name.Set.add name visited in
        Option.exists (Tenv.lookup tenv name) ~f:(fun ({fields} : Struct.t) ->
            List.exists fields ~f:(fun ({typ} : Struct.field) ->
                match typ.Typ.desc with
                | Tptr _ ->
                    true
                | Tstruct name ->
                    visit visited name
                | _ ->
                    false ) )
    in
    visit Typ.Name.Set.empty class_name


  let has_destructor_effects tenv class_name =
    Tenv.find_cpp_destructor tenv class_name
    |> Option.exists ~f:(fun proc ->
        Option.exists (IRAttributes.load proc) ~f:(fun (attrs : ProcAttributes.t) ->
            attrs.is_defined || not attrs.is_cpp_implicit ) )


  let continue_with f (outcomes, non_disj) =
    List.fold outcomes ~init:([], non_disj) ~f:(fun (prior, non_disj) outcome ->
        let more, non_disj =
          match outcome with
          | Ok (ContinueProgram astate) ->
              f astate non_disj
          | Recoverable (ContinueProgram astate, errors) ->
              let more, non_disj = f astate non_disj in
              (List.map more ~f:(fun state -> PulseResult.append_errors errors state), non_disj)
          | _ ->
              ([outcome], non_disj)
        in
        (prior @ more, non_disj) )


  let dispose_elements vector : model =
   fun ({path; location; callee_procname} as model_data) astate non_disj ->
    match element_type callee_procname with
    | Some ({desc= Tstruct _} as typ) ->
        let ( let<*> ) x f = bind_sat_result non_disj (Sat x) f in
        let<*> astate, (size, _) =
          GenericArrayBackedCollection.to_internal_size_deref path Read location vector astate
        in
        if PulseArithmetic.is_known_zero astate size then (Basic.ok_continue astate, non_disj)
        else
          let<*> astate, array =
            GenericArrayBackedCollection.eval path Read location vector astate
          in
          let elements =
            Memory.fold_edges (fst array) astate ~init:[] ~f:(fun elements (access, element) ->
                match (access : Access.t) with
                | ArrayAccess (_, index) -> (
                  match PulseArithmetic.as_constant_q astate index with
                  | Some index
                    when Q.compare index Q.zero >= 0
                         && Option.value_map (PulseArithmetic.as_constant_q astate size)
                              ~default:true ~f:(fun size -> Q.compare index size < 0 ) ->
                      element :: elements
                  | _ ->
                      elements )
                | _ ->
                    elements )
            |> List.dedup_and_sort ~compare:(fun (a, _) (b, _) -> AbstractValue.compare a b)
          in
          let pointer_typ = Typ.mk (Tptr (typ, Pk_pointer)) in
          List.fold elements
            ~init:(Basic.ok_continue astate, non_disj)
            ~f:(fun outcomes element ->
              continue_with
                (fun astate non_disj ->
                  let arg =
                    { FuncArg.arg_payload= ValueOrigin.unknown element
                    ; typ= pointer_typ
                    ; exp= Exp.Var (Ident.create_fresh Ident.kprimed) }
                  in
                  Basic.free_or_delete `Delete CppDelete arg model_data astate non_disj )
                outcomes )
    | _ ->
        (Basic.ok_continue astate, non_disj)


  let destructor vector : model =
   fun model_data astate non_disj ->
    dispose_elements vector model_data astate non_disj
    |> continue_with (fun astate non_disj ->
        lift_model (destroy_storage vector) model_data astate non_disj )


  let reallocate_internal_array path trace vector (vector_f : Invalidation.std_vector_function)
      location astate =
    let* astate, array_address =
      GenericArrayBackedCollection.eval path NoAccess location vector astate
    in
    let astate =
      match vector_f with
      | Clear ->
          astate
      | ShrinkToFit ->
          let tracked = Option.is_some (capacity_lower_bound vector astate) in
          let size =
            Memory.find_edge_opt (fst vector) (FieldAccess GenericArrayBackedCollection.size_field)
              astate
            |> Option.bind ~f:(fun (pointer, _) -> Memory.find_edge_opt pointer Dereference astate)
          in
          let capacity =
            if tracked then
              Option.bind size ~f:(fun (size, _) -> PulseArithmetic.as_constant_q astate size)
            else None
          in
          record_capacity vector capacity astate
      | _ ->
          record_capacity vector None astate
    in
    PulseOperations.invalidate_array_elements path location (StdVector vector_f) array_address
      astate
    >>| PulseOperations.invalidate_deref_access path location (StdVector vector_f) vector
          GenericArrayBackedCollection.access
    >>= PulseOperations.havoc_deref_field path location vector GenericArrayBackedCollection.field
          trace


  let shallow_copy_init_list path location this init_list ~desc astate =
    let event = Hist.call_event path location desc in
    let* astate, init_copy = PulseOperations.shallow_copy path location init_list astate in
    PulseOperations.write_deref_field path location ~ref:this GenericArrayBackedCollection.field
      ~obj:(fst init_copy, Hist.add_event event (snd init_copy))
      astate


  let init_list_constructor this init_list ~desc : model_no_non_disj =
   fun {path; location} astate ->
    let astate = record_capacity this None astate in
    let size = post_size InitializerList.size_field init_list astate in
    let<**> astate =
      match size with
      | Some size ->
          Sat (GenericArrayBackedCollection.assign_size path location this size ~desc astate)
      | None ->
          GenericArrayBackedCollection.assign_size_constant path location this ~constant:IntLit.zero
            ~desc astate
    in
    let capacity =
      Option.bind size ~f:(fun (size, _) -> PulseArithmetic.as_constant_q astate size)
    in
    let astate = record_capacity this capacity astate in
    let<+> astate = shallow_copy_init_list path location this init_list ~desc astate in
    astate


  let is_move_constructor callee_procname =
    List.nth (IRAttributes.load_formal_types callee_procname) 1
    |> Option.exists ~f:Typ.is_rvalue_reference


  let init_copy_constructor this init_vector ~desc : model_no_non_disj =
   fun {path; location; callee_procname} astate ->
    let move = is_move_constructor callee_procname in
    let astate =
      record_capacity this (if move then capacity_lower_bound init_vector astate else None) astate
    in
    let<*> astate, init_list =
      PulseOperations.eval_deref_access path Read location init_vector
        (FieldAccess GenericArrayBackedCollection.field) astate
    in
    let<*> astate, other_size =
      GenericArrayBackedCollection.to_internal_size_deref path Read location init_vector astate
    in
    let<*> astate =
      PulseOperations.write_deref_field path location ~ref:this
        GenericArrayBackedCollection.size_field ~obj:other_size astate
    in
    (* a move keeps the internal array so references to its elements stay valid; a copy also
       copies the elements, otherwise invalidating those of one vector would invalidate the other's *)
    let<*> astate, (copy, _) =
      if move then Ok (astate, init_list)
      else PulseOperations.deep_copy ~depth_max:1 path location init_list astate
    in
    (* Iterator witnesses belong to the source buffer. A copy gets new elements and must not
       retain witnesses whose invalidation would affect iterators into the source. *)
    let astate =
      if move then astate
      else
        let edges =
          Memory.fold_edges copy astate ~init:UnsafeMemory.Edges.empty
            ~f:(fun edges (access, value) ->
              match (access : Access.t) with
              | FieldAccess field
                when Fieldname.equal field
                       GenericArrayBackedCollection.Iterator.vector_positions_field ->
                  edges
              | _ ->
                  UnsafeMemory.Edges.add access value edges )
        in
        AbductiveDomain.set_post_edges copy edges astate
    in
    let<*> astate =
      PulseOperations.write_deref_field path location ~ref:this GenericArrayBackedCollection.field
        ~obj:(copy, Hist.add_call path location desc (snd init_list))
        astate
    in
    if move then
      let astate = record_capacity init_vector None astate in
      let<*> astate =
        PulseOperations.havoc_deref_field path location init_vector
          GenericArrayBackedCollection.field
          (Hist.single_call path location desc)
          astate
      in
      let<++> astate =
        GenericArrayBackedCollection.assign_size_constant path location init_vector
          ~constant:IntLit.zero ~desc astate
      in
      astate
    else Basic.ok_continue astate


  let invalidate_references vector_f vector : model_no_non_disj =
   fun {path; location} astate ->
    let event =
      Hist.call_event path location
        (Format.asprintf "%a()" Invalidation.pp_std_vector_function vector_f)
    in
    let<+> astate =
      reallocate_internal_array path (Hist.single_event event) vector vector_f location astate
    in
    astate


  let havoc_size path location vector ~desc astate =
    GenericArrayBackedCollection.assign_size path location vector
      (AbstractValue.mk_fresh (), ValueHistory.epoch)
      ~desc astate


  let invalidate_references_unknown_size vector_f vector : model_no_non_disj =
   fun ({path; location} as model_data) astate ->
    let desc = Format.asprintf "%a()" Invalidation.pp_std_vector_function vector_f in
    let<*> astate = havoc_size path location vector ~desc astate in
    invalidate_references vector_f vector model_data astate


  let invalidate_references_one_more vector_f vector : model_no_non_disj =
   fun ({path; location} as model_data) astate ->
    let desc = Format.asprintf "%a()" Invalidation.pp_std_vector_function vector_f in
    let<**> astate = GenericArrayBackedCollection.increase_size path location vector ~desc astate in
    invalidate_references vector_f vector model_data astate


  let assign_known_size vector size ~desc model_data astate =
    match size with
    | Some size ->
        GenericArrayBackedCollection.assign_size model_data.path model_data.location vector size
          ~desc astate
    | None ->
        havoc_size model_data.path model_data.location vector ~desc astate


  let assign_initializer_list vector list : model_no_non_disj =
   fun model_data astate ->
    let size = post_size InitializerList.size_field list astate in
    let<*> astate = assign_known_size vector size ~desc:"std::vector::assign()" model_data astate in
    invalidate_references Assign vector model_data astate


  let insert_count vector (count : _ FuncArg.t) : model_no_non_disj =
   fun ({path; location} as model_data) astate ->
    let rec literal = function
      | Exp.Cast (_, exp) ->
          literal exp
      | Exp.Const (Cint _) ->
          true
      | _ ->
          false
    in
    if
      (not (Typ.is_int count.typ))
      || (not (literal count.exp))
      || Option.is_none (capacity_lower_bound vector astate)
    then invalidate_references_unknown_size Insert vector model_data astate
    else
      let<**> astate =
        match post_size GenericArrayBackedCollection.size_field vector astate with
        | None ->
            Sat (havoc_size path location vector ~desc:"std::vector::insert()" astate)
        | Some (old, hist) ->
            let size = AbstractValue.mk_fresh () in
            let** astate, size =
              PulseArithmetic.eval_binop_absval size (PlusA None) old (fst count.arg_payload) astate
            in
            Sat
              (GenericArrayBackedCollection.assign_size path location vector (size, hist)
                 ~desc:"std::vector::insert()" astate )
      in
      invalidate_references Insert vector model_data astate


  let insert_initializer_list vector list : model_no_non_disj =
   fun ({path; location} as model_data) astate ->
    let<**> astate =
      match
        ( ( if Option.is_some (capacity_lower_bound vector astate) then
              post_size GenericArrayBackedCollection.size_field vector astate
            else None )
        , post_size InitializerList.size_field list astate )
      with
      | Some (old, hist), Some (count, _) ->
          let size = AbstractValue.mk_fresh () in
          let** astate, size =
            PulseArithmetic.eval_binop_absval size (PlusA None) old count astate
          in
          Sat
            (GenericArrayBackedCollection.assign_size path location vector (size, hist)
               ~desc:"std::vector::insert()" astate )
      | _ ->
          Sat (havoc_size path location vector ~desc:"std::vector::insert()" astate)
    in
    invalidate_references Insert vector model_data astate


  let assign_count vector FuncArg.{arg_payload= count; typ} : model_no_non_disj =
   fun ({path; location} as model_data) astate ->
    (* [assign(first, last)] has the same arity *)
    if Typ.is_int typ then
      let<*> astate =
        GenericArrayBackedCollection.assign_size path location vector count
          ~desc:"std::vector::assign()" astate
      in
      invalidate_references Assign vector model_data astate
    else invalidate_references_unknown_size Assign vector model_data astate


  let invalidate_references_with_ret vector_f vector : model_no_non_disj =
   fun ({ret= ret_id, _} as model_data) astate ->
    PulseOperations.write_id ret_id vector astate
    |> invalidate_references vector_f vector model_data


  let assign vector (rhs : _ FuncArg.t) : model =
   fun ({path; location} as model_data) astate non_disj ->
    if is_move_constructor model_data.callee_procname then
      let ( let<*> ) x f = bind_sat_result non_disj (Sat x) f in
      let<*> astate, array =
        GenericArrayBackedCollection.eval path Read location rhs.arg_payload astate
      in
      let<*> astate, size =
        GenericArrayBackedCollection.to_internal_size_deref path Read location rhs.arg_payload
          astate
      in
      lift_model (invalidate_references Assign vector) model_data astate non_disj
      |> continue_with (fun astate non_disj ->
          let ( let<*> ) x f = bind_sat_result non_disj (Sat x) f in
          let ( let<**> ) x f = bind_sat_result non_disj x f in
          let<*> astate =
            PulseOperations.write_deref_field path location ~ref:vector
              GenericArrayBackedCollection.field ~obj:array astate
          in
          let<*> astate =
            GenericArrayBackedCollection.assign_size path location vector size
              ~desc:"std::vector::operator=()" astate
          in
          let astate = record_capacity rhs.arg_payload None astate in
          let<*> astate =
            PulseOperations.havoc_deref_field path location rhs.arg_payload
              GenericArrayBackedCollection.field
              (Hist.single_call path location "std::vector::operator=()")
              astate
          in
          let<**> astate =
            GenericArrayBackedCollection.assign_size_constant path location rhs.arg_payload
              ~constant:IntLit.zero ~desc:"std::vector::operator=()" astate
          in
          (Basic.ok_continue (PulseOperations.write_id (fst model_data.ret) vector astate), non_disj) )
    else
      let ( let<*> ) x f = bind_sat_result non_disj (Sat x) f in
      let rec object_typ typ =
        match typ.Typ.desc with Tptr (typ, _) -> object_typ typ | _ -> typ
      in
      let size =
        if InitializerList.is_type (object_typ rhs.typ) then
          post_size InitializerList.size_field rhs.arg_payload astate
        else if Option.is_some (capacity_lower_bound rhs.arg_payload astate) then
          post_size GenericArrayBackedCollection.size_field rhs.arg_payload astate
        else None
      in
      let<*> astate =
        match size with
        | Some size ->
            GenericArrayBackedCollection.assign_size path location vector size
              ~desc:"std::vector::operator=()" astate
        | None ->
            Ok astate
      in
      lift_model (invalidate_references_with_ret Assign vector) model_data astate non_disj


  let at ~desc vector index : model_no_non_disj =
   fun {path; location; ret} astate ->
    let event = Hist.call_event path location desc in
    let<+> astate, (addr, hist) =
      GenericArrayBackedCollection.element path location vector (fst index) astate
    in
    PulseOperations.write_id (fst ret) (addr, Hist.add_event event hist) astate


  let last_index path location vector astate =
    let=* astate, (size, _) =
      GenericArrayBackedCollection.to_internal_size_deref path Read location vector astate
    in
    PulseArithmetic.eval_binop (AbstractValue.mk_fresh ()) (MinusA None) (AbstractValueOperand size)
      (ConstOperand (Cint IntLit.one)) astate


  (* the return type is checked because [std::vector<bool>] can return its elements by value *)
  let front ~desc vector : model_no_non_disj =
   fun ({ret= _, ret_typ} as model_data) astate ->
    if Typ.is_pointer ret_typ then
      let index_zero = AbstractValue.mk_fresh () in
      let<**> astate = PulseArithmetic.and_eq_int index_zero IntLit.zero astate in
      at ~desc vector (index_zero, ValueHistory.epoch) model_data astate
    else Basic.nondet ~desc model_data astate


  let back ~desc vector : model_no_non_disj =
   fun ({path; location; ret= _, ret_typ} as model_data) astate ->
    if Typ.is_pointer ret_typ then
      let<**> astate, last = last_index path location vector astate in
      at ~desc vector (last, ValueHistory.epoch) model_data astate
    else Basic.nondet ~desc model_data astate


  let data ~desc vector : model_no_non_disj =
   fun {path; location; ret= ret_id, _} astate ->
    let event = Hist.call_event path location desc in
    let<+> astate, (arr_addr, arr_hist) =
      GenericArrayBackedCollection.eval path Read location vector astate
    in
    PulseOperations.write_id ret_id (arr_addr, Hist.add_event event arr_hist) astate


  let vector_begin ~desc vector iter : model_no_non_disj =
   fun {path; location} astate ->
    let event = Hist.call_event path location desc in
    let index_zero = AbstractValue.mk_fresh () in
    let<**> astate = PulseArithmetic.and_eq_int index_zero IntLit.zero astate in
    let<+> astate =
      GenericArrayBackedCollection.Iterator.point_into path location event ~collection:vector ~iter
        ~index:index_zero ~track_vector_index:(tracked_positions vector astate) astate
    in
    astate


  let vector_end ~desc vector iter : model_no_non_disj =
   fun {path; location} astate ->
    let event = Hist.call_event path location desc in
    let<*> astate, (arr_addr, _) =
      GenericArrayBackedCollection.eval path Read location vector astate
    in
    let<*> astate, (pointer_addr, _) =
      GenericArrayBackedCollection.eval_pointer_to_last_element path location vector astate
    in
    let pointer_hist = Hist.add_event event (snd iter) in
    let pointer_val = (pointer_addr, pointer_hist) in
    let<*> astate =
      PulseOperations.write_deref_field path location ~ref:iter GenericArrayBackedCollection.field
        ~obj:(arr_addr, pointer_hist) astate
    in
    let<*> astate =
      PulseOperations.write_field path location ~ref:iter
        GenericArrayBackedCollection.Iterator.internal_pointer ~obj:pointer_val astate
    in
    match post_size GenericArrayBackedCollection.size_field vector astate with
    | None ->
        Basic.ok_continue astate
    | Some _ when not (tracked_positions vector astate) ->
        Basic.ok_continue astate
    | Some size ->
        let<*> astate, position =
          PulseOperations.eval_access path NoAccess location pointer_val Dereference astate
        in
        let<+> astate =
          GenericArrayBackedCollection.Iterator.record_vector_index path location position size
            astate
        in
        astate


  let clear_storage vector : model_no_non_disj =
   fun ({path; location} as model_data) astate ->
    let<**> astate =
      GenericArrayBackedCollection.assign_size_constant path location vector ~constant:IntLit.zero
        ~desc:"std::vector::clear()" astate
    in
    invalidate_references Clear vector model_data astate


  let clear vector : model =
   fun model_data astate non_disj ->
    dispose_elements vector model_data astate non_disj
    |> continue_with (fun astate non_disj ->
        lift_model (clear_storage vector) model_data astate non_disj )


  let rec is_integer_literal = function
    | Exp.Const (Cint _) ->
        true
    | Exp.Cast (_, exp) ->
        is_integer_literal exp
    | _ ->
        false


  let reserve_storage vector (requested : _ FuncArg.t) : model_no_non_disj =
   fun {path; location} astate ->
    let capacity = capacity_lower_bound vector astate in
    (* Other vector models can understate size (notably initializer-list construction). Do not
       prove a no-op from an arithmetic result or establish a growth bound on an untracked size. *)
    let requested_constant =
      if is_integer_literal requested.exp then
        PulseArithmetic.as_constant_q astate (fst requested.arg_payload)
      else None
    in
    let no_reallocation =
      match (capacity, requested_constant) with
      | Some capacity, Some requested ->
          Q.compare requested Q.zero >= 0 && Q.compare requested capacity <= 0
      | _ ->
          false
    in
    if no_reallocation then Basic.ok_continue astate
    else
      let hist = Hist.single_call path location "std::vector::reserve()" in
      let<+> astate =
        reallocate_internal_array path hist vector Reserve location astate
        >>| AddressAttributes.std_vector_reserve (fst vector)
      in
      let bound =
        match (capacity, requested_constant) with Some _, Some _ -> requested_constant | _ -> None
      in
      record_capacity vector bound astate


  (* A direct capacity()+1 argument cannot be a non-reallocating reserve. Recognize only adjacent
     calls in one CFG node with identical receivers and no intervening writes/calls. *)
  let immediate_capacity_growth (model_data : model_data) (requested : _ FuncArg.t) =
    let rec uncast (exp : Exp.t) = match exp with Exp.Cast (_, exp) -> uncast exp | exp -> exp in
    let one exp = match uncast exp with Exp.Const (Cint n) -> IntLit.isone n | _ -> false in
    let capacity_id =
      match uncast requested.exp with
      | Exp.BinOp (PlusA _, left, right) -> (
        match (uncast left, uncast right) with
        | Exp.Var id, _ when one right ->
            Some id
        | _, Exp.Var id when one left ->
            Some id
        | _ ->
            None )
      | _ ->
          None
    in
    Option.exists capacity_id ~f:(fun capacity_id ->
        List.exists (Procdesc.get_nodes model_data.analysis_data.proc_desc) ~f:(fun node ->
            let same_receiver loads (left : Exp.t) (right : Exp.t) =
              let left, right = (uncast left, uncast right) in
              Exp.equal left right
              ||
              match (left, right) with
              | Exp.Var left, Exp.Var right ->
                  Option.exists (Ident.Map.find_opt left loads) ~f:(fun source ->
                      Option.exists (Ident.Map.find_opt right loads) ~f:(Exp.equal source) )
              | _ ->
                  false
            in
            let supported_handler =
              (not
                 (Instrs.exists (Procdesc.Node.get_instrs node) ~f:(function
                   | Sil.Call (_, Exp.Const (Cfun proc), _, _, _) ->
                       Procname.is_destructor proc
                   | _ ->
                       false )) )
              && (not (List.is_empty (Procdesc.Node.get_exn node)))
              &&
              let visited = ref Procdesc.NodeSet.empty in
              let rec clear_path node =
                if Procdesc.NodeSet.mem node !visited then true
                else (
                  visited := Procdesc.NodeSet.add node !visited ;
                  if
                    Instrs.exists (Procdesc.Node.get_instrs node) ~f:(function
                      | Sil.Metadata (TryExit _) ->
                          true
                      | _ ->
                          false )
                  then true
                  else if
                    Instrs.exists (Procdesc.Node.get_instrs node) ~f:(function
                      | Sil.Call (_, Exp.Const (Cfun proc), _, _, _) ->
                          Procname.is_destructor proc
                      | _ ->
                          false )
                  then false
                  else List.for_all (Procdesc.Node.get_succs node) ~f:clear_path )
              in
              List.for_all (Procdesc.Node.get_succs node) ~f:clear_path
            in
            let rec scan loads previous = function
              | [] ->
                  false
              | Sil.Call ((ret, _), Exp.Const (Cfun callee), [(receiver, _); (count, _)], loc, _)
                :: rest
                when Ident.equal ret (fst model_data.ret)
                     && Procname.equal callee model_data.callee_procname
                     && Location.equal loc model_data.location
                     && Exp.equal count requested.exp -> (
                match previous with
                | Some (id, capacity_callee, capacity_receiver)
                  when Ident.equal id capacity_id
                       && String.equal (Procname.get_method capacity_callee) "capacity"
                       && Option.equal Typ.Name.equal
                            (Procname.get_class_type_name capacity_callee)
                            (Procname.get_class_type_name callee)
                       && same_receiver loads receiver capacity_receiver ->
                    true
                | _ ->
                    scan Ident.Map.empty None rest )
              | Sil.Call ((ret, _), Exp.Const (Cfun callee), [(receiver, _)], _, _) :: rest ->
                  if String.equal (Procname.get_method callee) "capacity" then
                    scan loads (Some (ret, callee, receiver)) rest
                  else scan Ident.Map.empty None rest
              | Sil.Load {id; e} :: rest ->
                  scan (Ident.Map.add id e loads) previous rest
              | Sil.Metadata _ :: rest ->
                  scan loads previous rest
              | _ :: rest ->
                  scan Ident.Map.empty None rest
            in
            supported_handler
            && scan Ident.Map.empty None
                 (Array.to_list
                    (Instrs.get_underlying_not_reversed (Procdesc.Node.get_instrs node)) ) ) )


  let relocation_constructor (model_data : model_data) =
    let standard_allocator =
      let open ProcnameDispatcher.TypName in
      make_dispatcher [-"std" &:: "allocator" &::.*--> ()]
    in
    match Procname.get_class_type_name model_data.callee_procname with
    | Some
        (CppClass
           { template_spec_info=
               Template
                 {args= TType ({desc= Tstruct name} as typ) :: TType {desc= Tstruct allocator} :: _}
           } )
      when Option.is_some (standard_allocator () allocator)
           && Option.value_map
                ( Tenv.find_cpp_destructor model_data.analysis_data.tenv name
                |> Option.bind ~f:IRAttributes.load )
                ~default:false
                ~f:(fun attrs -> attrs.ProcAttributes.is_cpp_implicit) -> (
        let rec scalar typ =
          match typ.Typ.desc with
          | Tint _ | Tfloat _ ->
              true
          | Tarray {elt} ->
              scalar elt
          | _ ->
              false
        in
        if
          not
            (Option.exists (Tenv.lookup model_data.analysis_data.tenv name)
               ~f:(fun (info : Struct.t) ->
                 List.is_empty info.supers
                 && List.for_all info.fields ~f:(fun (field : Struct.field) -> scalar field.typ) )
            )
        then None
        else
          let constructors =
            Tenv.find_cpp_constructor model_data.analysis_data.tenv name
            |> List.filter_map ~f:(fun proc ->
                IRAttributes.load proc
                |> Option.map ~f:(fun (attrs : ProcAttributes.t) -> (proc, attrs)) )
          in
          let supported (attrs : ProcAttributes.t) =
            attrs.ProcAttributes.is_defined && (not attrs.is_cpp_deleted)
            && ProcAttributes.equal_access attrs.access Public
            && Int.equal (List.length attrs.formals) 2
            && Option.is_some attrs.cpp_constructor_is_nothrow
          in
          let copies =
            List.filter constructors ~f:(fun (_, (attrs : ProcAttributes.t)) ->
                attrs.is_cpp_copy_ctor && supported attrs
                && List.mem attrs.const_formals 1 ~equal:Int.equal )
          in
          let moves =
            List.filter constructors ~f:(fun (_, (attrs : ProcAttributes.t)) ->
                (not attrs.is_cpp_deleted)
                && ( attrs.is_cpp_move_ctor
                   || List.exists attrs.formals ~f:(fun (_, formal, _) ->
                       Typ.equal formal (Typ.mk (Tptr (typ, Pk_rvalue_reference))) ) ) )
          in
          match (copies, moves) with
          | [copy], [] ->
              Some (typ, copy)
          | [copy], [((_, attrs) as move)] when attrs.is_cpp_move_ctor && supported attrs -> (
            match attrs.cpp_constructor_is_nothrow with
            | Some true ->
                Some (typ, move)
            | Some false ->
                Some (typ, copy)
            | None ->
                None )
          | _ ->
              None )
    | _ ->
        None


  let reserve vector requested : model =
   fun ({path; location} as model_data) astate non_disj ->
    let fallback = lift_model (reserve_storage vector requested) in
    let tracked_singleton =
      Option.is_some (capacity_lower_bound vector astate)
      && Option.exists
           ( Memory.find_edge_opt (fst vector) (FieldAccess GenericArrayBackedCollection.size_field)
               astate
           |> Option.bind ~f:(fun (pointer, _) -> Memory.find_edge_opt pointer Dereference astate)
           )
           ~f:(fun (size, _) ->
             Option.exists (PulseArithmetic.as_constant_q astate size) ~f:(Q.equal Q.one) )
    in
    if (not tracked_singleton) || not (immediate_capacity_growth model_data requested) then
      fallback model_data astate non_disj
    else
      match relocation_constructor model_data with
      | None ->
          fallback model_data astate non_disj
      | Some (typ, (constructor, attrs))
        when Option.exists (Procdesc.load constructor) ~f:(fun desc ->
                 not
                   (Procdesc.fold_instrs desc ~init:false ~f:(fun found _ instr ->
                        found || match instr with Sil.Metadata (TryEntry _) -> true | _ -> false )
                   ) ) ->
          let ( let<**> ) x f = bind_sat_result non_disj x f in
          let ( let<*> ) x f = bind_sat_result non_disj (Sat x) f in
          let<**> astate, dst =
            Basic.alloc_value_address typ ~desc:"std::vector::reserve()" model_data astate
          in
          let index = AbstractValue.mk_fresh () in
          let<**> astate = PulseArithmetic.and_eq_int index IntLit.zero astate in
          let<*> astate, source =
            GenericArrayBackedCollection.element path location vector index astate
          in
          let<**> astate = PulseArithmetic.and_positive (fst source) astate in
          let arg value typ =
            { FuncArg.arg_payload= ValueOrigin.unknown value
            ; typ
            ; exp= Exp.Var (Ident.create_fresh Ident.kprimed) }
          in
          let this_type, source_type =
            match attrs.formals with
            | [(_, this_type, _); (_, source_type, _)] ->
                (this_type, source_type)
            | _ ->
                assert false
          in
          model_data.dispatch_call_eval_args model_data.analysis_data path model_data.ret
            (Const (Cfun constructor))
            [arg dst this_type; arg source source_type]
            location CallFlags.default astate non_disj (Some constructor)
          |> fun (outcomes, non_disj) ->
          let outcomes =
            if Option.value attrs.cpp_constructor_is_nothrow ~default:false then
              List.concat_map outcomes ~f:(function
                | Ok (ExceptionRaised astate) ->
                    Basic.early_exit model_data astate
                | Recoverable (ExceptionRaised astate, errors) ->
                    Basic.early_exit model_data astate
                    |> List.map ~f:(fun state -> PulseResult.append_errors errors state)
                | state ->
                    [state] )
            else outcomes
          in
          let unsupported_exit =
            Option.equal Bool.equal attrs.cpp_constructor_is_nothrow (Some false)
            && List.exists outcomes ~f:(function
              | Ok (Stopped (ExitProgram _)) | Recoverable (Stopped (ExitProgram _), _) ->
                  true
              | _ ->
                  false )
          in
          if unsupported_exit then fallback model_data astate non_disj
          else
            continue_with
              (fun astate non_disj ->
                fallback model_data astate non_disj
                |> continue_with (fun astate non_disj ->
                    let ( let<*> ) x f = bind_sat_result non_disj (Sat x) f in
                    let<*> astate, array =
                      GenericArrayBackedCollection.eval path Read location vector astate
                    in
                    let astate =
                      Memory.add_edge path array
                        (ArrayAccess (StdTyp.void, index))
                        dst location astate
                    in
                    (Basic.ok_continue astate, non_disj) ) )
              (outcomes, non_disj)
      | Some _ ->
          fallback model_data astate non_disj


  let pop_storage vector ~desc : model_no_non_disj =
   fun {path; location} astate ->
    let<**> astate, last = last_index path location vector astate in
    let<*> astate, array = GenericArrayBackedCollection.eval path Read location vector astate in
    let<*> astate, element =
      GenericArrayBackedCollection.eval_element path location array last astate
    in
    let astate = invalidate_element path location (StdVector PopBack) element astate in
    let astate =
      Memory.add_edge path array
        (ArrayAccess (StdTyp.void, last))
        (AbstractValue.mk_fresh (), Hist.single_call path location desc)
        location astate
    in
    let<++> astate = GenericArrayBackedCollection.decrease_size path location vector ~desc astate in
    astate


  let pop_back vector ~desc : model =
   fun ({path; location; callee_procname} as model_data) astate non_disj ->
    let finish astate non_disj = lift_model (pop_storage vector ~desc) model_data astate non_disj in
    match element_type callee_procname with
    | Some ({desc= Tstruct _} as typ) ->
        let ( let<**> ) x f = bind_sat_result non_disj x f in
        let ( let<*> ) x f = bind_sat_result non_disj (Sat x) f in
        let<**> astate, last = last_index path location vector astate in
        let<*> astate, element =
          GenericArrayBackedCollection.element path location vector last astate
        in
        let arg =
          { FuncArg.arg_payload= ValueOrigin.unknown element
          ; typ= Typ.mk (Tptr (typ, Pk_pointer))
          ; exp= Exp.Var (Ident.create_fresh Ident.kprimed) }
        in
        Basic.free_or_delete `Delete CppDelete arg model_data astate non_disj
        |> continue_with finish
    | _ ->
        finish astate non_disj


  let reallocate_unless_reserved ?size path location vector vector_f ~desc astate =
    match vector_f with
    | None ->
        Ok astate
    | Some vector_f ->
        let capacity = capacity_lower_bound vector astate in
        let* astate, size =
          match size with
          | Some size ->
              Ok (astate, size)
          | None ->
              GenericArrayBackedCollection.to_internal_size_deref path Read location vector astate
        in
        let size_constant = PulseArithmetic.as_constant_q astate (fst size) in
        let reserved = AddressAttributes.is_std_vector_reserved (fst vector) astate in
        let exceeds_bound =
          match (capacity, size_constant) with
          | Some capacity, Some size ->
              Q.compare size capacity > 0
          | _ ->
              false
        in
        let within_bound =
          match (capacity, size_constant) with
          | Some capacity, Some size ->
              Q.compare size capacity <= 0
          | _ ->
              false
        in
        let skip_reallocation = within_bound || (reserved && not exceeds_bound) in
        let* astate =
          if skip_reallocation then
            (* Unknown growth retains the existing reserve approximation. Its capacity proof is
               forgotten below, so a later reserve cannot silently rely on skipped growth. *)
            Ok astate
          else
            reallocate_internal_array path
              (Hist.single_call path location desc)
              vector vector_f location astate
        in
        (* Write even when the callee has no bound: a caller's bound must not survive unsupported
           summary growth. Successful reallocation establishes capacity >= the resulting size. *)
        let bound =
          if skip_reallocation then
            match (capacity, size_constant) with Some _, Some _ -> capacity | _ -> None
          else if Option.is_some capacity then size_constant
          else None
        in
        Ok (record_capacity vector bound astate)


  let is_scalar typ = Typ.is_int typ || Typ.is_pointer typ

  (* The pushed value can be an element of the vector itself, so it is read before the vector
     reallocates. Elements of class type would need their constructor. *)
  let read_pushed_value path location callee_procname (value : _ FuncArg.t option) astate =
    match (Procname.get_class_type_name callee_procname, value) with
    | ( Some (CppClass {template_spec_info= Template {args= TType elem_typ :: _}})
      , Some
          { arg_payload
          ; typ= {Typ.desc= Tptr (value_typ, (Pk_lvalue_reference | Pk_rvalue_reference))} } )
      when is_scalar elem_typ && is_scalar value_typ ->
        let+ astate, v =
          PulseOperations.eval_access path Read location arg_payload Dereference astate
        in
        (astate, Some v)
    | _ ->
        Ok (astate, None)


  (* what the pushed value owns stays reachable from the vector *)
  let write_last_element path location vector pushed astate =
    match pushed with
    | None ->
        Sat (Ok astate)
    | Some v ->
        let** astate, last = last_index path location vector astate in
        Sat
          (let* astate, elem =
             GenericArrayBackedCollection.element path location vector last astate
           in
           PulseOperations.write_deref path location ~ref:elem ~obj:v astate )


  let push_back_common ?value vector ~vector_f ~desc : model_no_non_disj =
   fun {path; location; callee_procname; ret= ret_id, _} astate ->
    let<*> astate, pushed = read_pushed_value path location callee_procname value astate in
    let<**> astate = GenericArrayBackedCollection.increase_size path location vector ~desc astate in
    let<*> astate = reallocate_unless_reserved path location vector vector_f ~desc astate in
    let<++> astate = write_last_element path location vector pushed astate in
    PulseOperations.write_id ret_id
      (fst vector, Hist.add_call path location desc (snd vector))
      astate


  let push_back_cpp vector (value : _ FuncArg.t) ~vector_f ~desc : model =
   fun ({path; location; callee_procname; analysis_data= {tenv}} as model_data) astate non_disj ->
    let fallback = lift_model (push_back_common ~value vector ~vector_f:(Some vector_f) ~desc) in
    match element_type callee_procname with
    | Some ({desc= Tstruct class_name} as typ)
      when (not (Tenv.is_trivially_copyable tenv typ))
           && has_destructor_effects tenv class_name
           && has_pointer_members tenv class_name ->
        let pointer_typ = Typ.mk (Tptr (typ, Pk_pointer)) in
        let source_typ =
          match IRAttributes.load_formal_types callee_procname with
          | [_; typ] ->
              typ
          | _ ->
              value.FuncArg.typ
        in
        let actuals = [pointer_typ; source_typ] in
        let constructor =
          List.find_map Typ.overloading_resolution ~f:(fun compare ->
              List.find (Tenv.find_cpp_constructor tenv class_name) ~f:(fun proc ->
                  (not
                     (Option.exists (IRAttributes.load proc) ~f:(fun (attrs : ProcAttributes.t) ->
                          attrs.is_cpp_deleted ) ) )
                  && List.equal compare (IRAttributes.load_formal_types proc) actuals ) )
        in
        if Option.is_none constructor then fallback model_data astate non_disj
        else
          let ( let<**> ) x f = bind_sat_result non_disj x f in
          let<**> astate, dst = Basic.alloc_value_address typ ~desc model_data astate in
          let fake_exp = Exp.Var (Ident.create_fresh Ident.kprimed) in
          let args =
            [ {FuncArg.arg_payload= ValueOrigin.unknown dst; typ= pointer_typ; exp= fake_exp}
            ; FuncArg.map_payload ~f:ValueOrigin.unknown {value with typ= source_typ} ]
          in
          let constructor = Option.value_exn constructor in
          model_data.dispatch_call_eval_args model_data.analysis_data path model_data.ret
            (Const (Cfun constructor)) args location CallFlags.default astate non_disj
            (Some constructor)
          |> continue_with (fun astate non_disj ->
              let ( let<**> ) x f = bind_sat_result non_disj x f in
              let ( let<*> ) x f = bind_sat_result non_disj (Sat x) f in
              let<**> astate =
                GenericArrayBackedCollection.increase_size path location vector ~desc astate
              in
              let<*> astate =
                reallocate_unless_reserved path location vector (Some vector_f) ~desc astate
              in
              let<**> astate, last = last_index path location vector astate in
              let<*> astate, array =
                GenericArrayBackedCollection.eval path Read location vector astate
              in
              let astate =
                Memory.add_edge path array (ArrayAccess (StdTyp.void, last)) dst location astate
              in
              ( Basic.ok_continue (PulseOperations.write_id (fst model_data.ret) vector astate)
              , non_disj ) )
    | _ ->
        fallback model_data astate non_disj


  let push_back vector ~desc = push_back_common vector ~vector_f:None ~desc

  let emplace_back vector args ~desc : model_no_non_disj =
   fun {path; location; callee_procname; ret= ret_id, _} astate ->
    let<*> astate, pushed =
      match args with
      | [value] ->
          read_pushed_value path location callee_procname (Some value) astate
      | _ ->
          Ok (astate, None)
    in
    let<**> astate = GenericArrayBackedCollection.increase_size path location vector ~desc astate in
    let<*> astate =
      reallocate_unless_reserved path location vector (Some EmplaceBack) ~desc astate
    in
    let<**> astate = write_last_element path location vector pushed astate in
    (* since C++17 the new element is returned by reference *)
    let<**> astate, last = last_index path location vector astate in
    let<+> astate, (elem, _) =
      GenericArrayBackedCollection.element path location vector last astate
    in
    PulseOperations.write_id ret_id (elem, Hist.single_call path location desc) astate


  let resize vector (requested : _ FuncArg.t) : model_no_non_disj =
   fun {path; location} astate ->
    let size = requested.arg_payload in
    let astate =
      if is_integer_literal requested.exp then astate else record_capacity vector None astate
    in
    let desc = "std::vector::resize()" in
    let<*> astate, (old_size, _) =
      GenericArrayBackedCollection.to_internal_size_deref path Read location vector astate
    in
    (* shrinking never reallocates; when the vector may grow, assume that it reallocates without
       splitting the state, as for [push_back] *)
    let may_grow =
      PulseArithmetic.prune_binop ~negated:true Le
        (AbstractValueOperand (fst size))
        (AbstractValueOperand old_size) astate
      |> SatUnsat.sat |> Option.is_some
    in
    let<*> astate =
      if may_grow then
        reallocate_unless_reserved ~size path location vector (Some Resize) ~desc astate
      else Ok astate
    in
    let<+> astate =
      GenericArrayBackedCollection.assign_size path location vector size ~desc astate
    in
    astate


  (* Iterators store the position of [begin()] as the element at index 0 (see [vector_begin]) plus
     an offset, whereas [operator[]] uses the index itself, and an element edge of the internal
     array does not say which of the two it uses, so try both. *)
  let is_before_position astate array_edges index position =
    let logical_position = GenericArrayBackedCollection.Iterator.vector_index position astate in
    let logical_index = GenericArrayBackedCollection.Iterator.vector_index index astate in
    let provably_lt astate lhs =
      PulseArithmetic.prune_binop ~negated:false Ge (AbstractValueOperand lhs)
        (AbstractValueOperand position) astate
      |> SatUnsat.sat |> Option.is_none
    in
    match logical_position with
    | Some (position, _) ->
        let index = Option.value_map logical_index ~default:index ~f:fst in
        PulseArithmetic.prune_binop ~negated:false Ge (AbstractValueOperand index)
          (AbstractValueOperand position) astate
        |> SatUnsat.sat |> Option.is_none
    | None ->
        provably_lt astate index
        || List.exists array_edges ~f:(fun (access, (first, _)) ->
            match (access : Access.t) with
            | ArrayAccess (_, zero) when PulseArithmetic.is_known_zero astate zero -> (
              match
                PulseArithmetic.eval_binop (AbstractValue.mk_fresh ()) (PlusA None)
                  (AbstractValueOperand first) (AbstractValueOperand index) astate
              with
              | Sat (Ok (astate, offset_from_first)) ->
                  provably_lt astate offset_from_first
              | Sat (Recoverable _ | FatalError _) | Unsat _ ->
                  false )
            | _ ->
                false )


  (** [erase] does not reallocate: the elements before [position] stay where they are and the others
      are invalidated; a fresh internal array keeps the old and new iterators apart *)
  let erase_from path location event vector position astate =
    let* astate, old_array =
      GenericArrayBackedCollection.eval path NoAccess location vector astate
    in
    let* astate = PulseOperations.check_addr_access path NoAccess location old_array astate in
    let* astate =
      PulseOperations.havoc_deref_field path location vector GenericArrayBackedCollection.field
        (Hist.single_event event) astate
    in
    let+ astate, new_array =
      GenericArrayBackedCollection.eval path NoAccess location vector astate
    in
    let astate =
      match GenericArrayBackedCollection.Iterator.vector_positions (fst old_array) astate with
      | None ->
          astate
      | Some registry ->
          let astate =
            Memory.fold_edges (fst registry) astate ~init:astate
              ~f:(fun astate (_, (index, hist)) ->
                if is_before_position astate [] index position then astate
                else
                  PulseOperations.invalidate path UntraceableAccess location (StdVector Erase)
                    (index, hist) astate )
          in
          Memory.add_edge path new_array
            (FieldAccess GenericArrayBackedCollection.Iterator.vector_positions_field) registry
            location astate
    in
    let array_edges = Memory.fold_edges (fst old_array) astate ~init:[] ~f:(Fn.flip List.cons) in
    List.fold array_edges ~init:astate ~f:(fun astate (access, elem) ->
        match (access : Access.t) with
        | ArrayAccess (_, index) when is_before_position astate array_edges index position ->
            Memory.add_edge path new_array access elem location astate
        | ArrayAccess _ ->
            PulseOperations.invalidate path
              (MemoryAccess {pointer= old_array; access; hist_obj_default= snd elem})
              location (StdVector Erase) elem astate
        | _ ->
            astate )
    (* for the callers, which do not know which of their elements are before [position] *)
    |> AddressAttributes.add_one (fst old_array)
         (StdVectorErased (Immediate {location; history= snd old_array}))


  let erase ~range vector first iter : model_no_non_disj =
   fun ({path; location} as model_data) astate ->
    let desc = "std::vector::erase()" in
    let event = Hist.call_event path location desc in
    let<**> astate =
      if range then SatUnsat.Sat (havoc_size path location vector ~desc astate)
      else GenericArrayBackedCollection.decrease_size path location vector ~desc astate
    in
    let<*> astate, pointer =
      GenericArrayBackedCollection.Iterator.to_internal_pointer path Read location first astate
    in
    let<*> astate, (position, _) =
      PulseOperations.eval_access path Read location pointer Dereference astate
    in
    let logical_position = GenericArrayBackedCollection.Iterator.vector_index position astate in
    let<*> astate = erase_from path location event vector position astate in
    match logical_position with
    | Some (index, _) ->
        let<+> astate =
          GenericArrayBackedCollection.Iterator.point_into path location event ~collection:vector
            ~iter ~index ~track_vector_index:true astate
        in
        astate
    | None ->
        (* the returned iterator is at the erased position, unless that position is the one of
       [begin()] and was invalidated above as the element at index 0 *)
        if
          AddressAttributes.find_opt `Post position astate
          |> Option.exists ~f:(fun attrs -> Attributes.get_invalid attrs |> Option.is_some)
        then vector_begin ~desc vector iter model_data astate
        else
          let<+> astate =
            GenericArrayBackedCollection.Iterator.point_at path location event ~collection:vector
              ~iter ~position astate
          in
          astate


  let insert_one vector first iter : model_no_non_disj =
   fun ({path; location} as model_data) astate ->
    let desc = "std::vector::insert()" in
    let no_reallocation =
      match
        ( capacity_lower_bound vector astate
        , post_size GenericArrayBackedCollection.size_field vector astate )
      with
      | Some capacity, Some (size, _) ->
          Option.exists (PulseArithmetic.as_constant_q astate size) ~f:(fun size ->
              Q.compare (Q.add size Q.one) capacity <= 0 )
      | _ ->
          false
    in
    if not no_reallocation then invalidate_references_one_more Insert vector model_data astate
    else
      let<*> astate, pointer =
        GenericArrayBackedCollection.Iterator.to_internal_pointer path Read location first astate
      in
      let<*> astate, position =
        PulseOperations.eval_access path Read location pointer Dereference astate
      in
      let at_end =
        AddressAttributes.is_end_of_collection (fst pointer) astate
        && Option.exists
             (GenericArrayBackedCollection.Iterator.vector_index (fst position) astate)
             ~f:(fun (index, _) ->
               Option.exists (post_size GenericArrayBackedCollection.size_field vector astate)
                 ~f:(fun (size, _) ->
                   PulseArithmetic.prune_binop ~negated:false Ne (AbstractValueOperand index)
                     (AbstractValueOperand size) astate
                   |> SatUnsat.sat |> Option.is_none ) )
      in
      if not at_end then invalidate_references_one_more Insert vector model_data astate
      else
        let old_size =
          post_size GenericArrayBackedCollection.size_field vector astate |> Option.value_exn
        in
        let<**> astate =
          GenericArrayBackedCollection.increase_size path location vector ~desc astate
        in
        (* An insertion at end with spare capacity keeps every element in place. Only the old
           past-the-end iterator is invalidated; a new one has a fresh position. *)
        let astate =
          PulseOperations.invalidate path UntraceableAccess location (StdVector Insert) position
            astate
        in
        let<*> astate =
          PulseOperations.havoc_deref_field path location vector
            GenericArrayBackedCollection.last_field
            (Hist.single_call path location desc)
            astate
        in
        let event = Hist.call_event path location desc in
        let<+> astate =
          GenericArrayBackedCollection.Iterator.point_into path location event ~collection:vector
            ~iter ~index:(fst old_size) ~track_vector_index:true astate
        in
        astate
end

(** A [std::span] is a view of a contiguous sequence it does not own. It gets the representation of
    the array-backed collections, with [GenericArrayBackedCollection.field] holding the array it
    views and [size_field] its extent. That array is the array of the sequence's owner, so
    destroying the owner, or reallocating its storage, invalidates accesses through the span too. *)
module Span = struct
  let size_access = Access.FieldAccess GenericArrayBackedCollection.size_field

  let get_array collection : PulseModelsDSL.aval PulseModelsDSL.model_monad =
    let open PulseModelsDSL.Syntax in
    load_access collection (FieldAccess GenericArrayBackedCollection.field)


  let get_size collection : PulseModelsDSL.aval PulseModelsDSL.model_monad =
    let open PulseModelsDSL.Syntax in
    let* size_pointer = access Read collection size_access in
    load size_pointer


  let set_view this ~array ~size : unit PulseModelsDSL.model_monad =
    let open PulseModelsDSL.Syntax in
    store_field ~ref:this GenericArrayBackedCollection.field array
    @@> let* size_pointer = access Read this size_access in
        store ~ref:size_pointer size


  (** the extent of [std::span<T, N>], taken from the template arguments of a constructor such as
      [span(T (&)[N])] and [span(std::array<T, N>&)], or from those of the class itself *)
  let static_extent callee_procname =
    let extent_of (template_spec_info : Typ.template_spec_info) =
      match template_spec_info with
      | NoTemplate ->
          None
      | Template {args} ->
          List.find_map args ~f:(function
            | Typ.TInt extent when Int64.is_non_negative extent ->
                Int64.to_int extent
            | _ ->
                None )
    in
    let of_method =
      match (callee_procname : Procname.t) with
      | ObjC_Cpp {template_args} ->
          extent_of template_args
      | _ ->
          None
    in
    match of_method with
    | Some _ as extent ->
        extent
    | None -> (
      match Procname.get_class_type_name callee_procname with
      | Some (CppClass {template_spec_info}) ->
          extent_of template_spec_info
      | _ ->
          None )


  (** the extent of an argument of C array type, as in [span(T (&arr)[N])] *)
  let rec extent_of_typ ({desc} : Typ.t) =
    match desc with
    | Tarray {length= Some length} ->
        IntLit.to_int length
    | Tptr (pointee, _) ->
        extent_of_typ pointee
    | _ ->
        None


  let extent_size extent : PulseModelsDSL.aval PulseModelsDSL.model_monad =
    let open PulseModelsDSL.Syntax in
    match extent with Some extent -> int extent | None -> fresh_nonneg ()


  let default_constructor ~desc this : model =
    let open PulseModelsDSL.Syntax in
    start_named_model desc
    @@ fun () ->
    let* array = fresh () in
    let* size = int 0 in
    set_view this ~array ~size


  let copy_view ~from:collection this : unit PulseModelsDSL.model_monad =
    let open PulseModelsDSL.Syntax in
    let* array = get_array collection in
    let* size = get_size collection in
    set_view this ~array ~size


  (** [std::span(C& c)] where [c] is another span or one of the array-backed collections: both view
      the same array, so the span sees the invalidations of the storage of [c] *)
  let constructor_from_collection ~desc this collection : model =
    let open PulseModelsDSL.Syntax in
    start_named_model desc @@ fun () -> copy_view ~from:collection this


  let assign ~desc this other : model =
    let open PulseModelsDSL.Syntax in
    start_named_model desc @@ fun () -> copy_view ~from:other this @@> assign_ret this


  (** [std::span(It first, size_type count)], and the iterator-pair constructor, whose extent is
      left unknown *)
  let constructor_from_iterator ~desc this first (count : PulseModelsDSL.aval FuncArg.t) : model =
    let open PulseModelsDSL.Syntax in
    start_named_model desc
    @@ fun () ->
    let* size = if Typ.is_int count.typ then ret count.arg_payload else fresh_nonneg () in
    set_view this ~array:first ~size


  (** [std::span(T (&arr)[N])], [std::span(std::array<T, N>& arr)] and the constructors from the
      sequences that Pulse does not model: the span views the argument itself *)
  let constructor_from_array ~desc this (arr : PulseModelsDSL.aval FuncArg.t) : model =
    let open PulseModelsDSL.Syntax in
    start_named_model desc
    @@ fun () ->
    let* {callee_procname} = get_data in
    let extent =
      match extent_of_typ arr.typ with
      | Some _ as extent ->
          extent
      | None ->
          static_extent callee_procname
    in
    let* size = extent_size extent in
    set_view this ~array:arr.arg_payload ~size


  let data ~desc this : model =
    let open PulseModelsDSL.Syntax in
    start_named_model desc @@ fun () -> get_array this >>= assign_ret


  (** a view that starts or ends outside the sequence the span covers: its array is invalid, so
      reading or writing through the view is reported *)
  let out_of_bounds result : unit PulseModelsDSL.model_monad =
    let open PulseModelsDSL.Syntax in
    let* {path; location} = get_data in
    let* array = fresh () in
    exec_command
      (PulseOperations.invalidate path UntraceableAccess location (OutOfBounds Span) array)
    @@> let* size = fresh_nonneg () in
        set_view result ~array ~size


  let offset_array array offset : PulseModelsDSL.aval PulseModelsDSL.model_monad =
    buffer_offset ~desc:"std::span offset" array offset


  (** Whether [request] is known to be past [limit]. Only constants are compared: assuming that a
      symbolic range is out of bounds would both report on the many correct programs whose extent
      Pulse cannot see and, because the assumption is made in the procedure itself, report it as a
      manifest issue rather than a latent one. *)
  let known_past ~request ~limit : bool PulseModelsDSL.model_monad =
    let open PulseModelsDSL.Syntax in
    let* request = as_constant_q request in
    let* limit = as_constant_q limit in
    match (request, limit) with
    | Some request, Some limit ->
        ret (Q.compare request limit > 0)
    | _ ->
        ret false


  (** [std::dynamic_extent], the default [count] of [subspan], is [size_t(-1)] *)
  let is_dynamic_extent count : bool PulseModelsDSL.model_monad =
    let open PulseModelsDSL.Syntax in
    let* count = as_constant_q count in
    match count with
    | Some count ->
        ret (Q.compare count Q.zero < 0 || Q.compare count (Q.of_bigint Z.(shift_left one 63)) >= 0)
    | None ->
        ret false


  (** [std::span::first(count)] *)
  let first ~desc this count result : model =
    let open PulseModelsDSL.Syntax in
    start_named_model desc
    @@ fun () ->
    let* size = get_size this in
    let* past_the_end = known_past ~request:count ~limit:size in
    if past_the_end then out_of_bounds result
    else
      let* array = get_array this in
      set_view result ~array ~size:count


  (** [std::span::last(count)] *)
  let last ~desc this count result : model =
    let open PulseModelsDSL.Syntax in
    start_named_model desc
    @@ fun () ->
    let* size = get_size this in
    let* past_the_end = known_past ~request:count ~limit:size in
    if past_the_end then out_of_bounds result
    else
      let* array = get_array this in
      let* offset = binop (MinusA None) size count in
      let* array = offset_array array offset in
      set_view result ~array ~size:count


  (** [std::span::subspan(offset, count)] *)
  let subspan ~desc this offset count result : model =
    let open PulseModelsDSL.Syntax in
    start_named_model desc
    @@ fun () ->
    let* size = get_size this in
    let* remaining = binop (MinusA None) size offset in
    let* to_the_end = is_dynamic_extent count in
    let* past_the_end = known_past ~request:offset ~limit:size in
    let* past_the_end =
      if past_the_end || to_the_end then ret past_the_end
      else known_past ~request:count ~limit:remaining
    in
    if past_the_end then out_of_bounds result
    else
      let* array = get_array this in
      let* array = offset_array array offset in
      set_view result ~array ~size:(if to_the_end then remaining else count)
end

module GenericMapCollection = struct
  let pair_field = Fieldname.make PulseOperations.pulse_model_type "__infer_map_pair"

  let pair_access = Access.FieldAccess pair_field

  let lookup_key_field = Fieldname.make PulseOperations.pulse_model_type "__infer_map_lookup_key"

  let lookup_present_field =
    Fieldname.make PulseOperations.pulse_model_type "__infer_map_lookup_present"


  let pair_type key_t value_t =
    Typ.CppClass
      { name= QualifiedCppName.of_qual_string "std::pair"
      ; template_spec_info=
          Template {mangled= None; args= [TType (Typ.set_to_const key_t); TType value_t]}
      ; is_union= false }


  let pair_first_field key_t value_t = Fieldname.make (pair_type key_t value_t) "first"

  let pair_first_access key_t value_t = Access.FieldAccess (pair_first_field key_t value_t)

  let pair_second_field key_t value_t = Fieldname.make (pair_type key_t value_t) "second"

  let pair_second_access key_t value_t = Access.FieldAccess (pair_second_field key_t value_t)

  let extract_key_and_value_types {FuncArg.typ= map_typ} =
    let rec extract_helper {Typ.desc} =
      match desc with
      | Tstruct (CppClass {template_spec_info= Template {args= TType key_t :: TType value_t :: _}})
        ->
          Some (key_t, value_t)
      | Tptr (typ, _) ->
          (* We strip [Tptr] recursively because there can be multiple wrappings of it, e.g. an
             rvalue-ref parameter can be captured by reference in lambda.  Also, the stripping
             semantics is aligned with the [ProcnameDispatcher.match_typ] definition. *)
          extract_helper typ
      | _ ->
          (* This should never happen, as we already know from using [capt_arg_of_typ] that map
             is of some map type, hence it should have a first and second template arguments
             mapping to key and value types respectively. *)
          L.internal_error "Unexpected (key, value) template type: %a@\n" (Typ.pp_full Pp.text)
            map_typ ;
          None
    in
    extract_helper map_typ


  let unwrap_pointer_to_struct_type (typ : Typ.t) =
    match typ.desc with Tptr ({desc= Tstruct pointee}, _) -> pointee | _ -> assert false


  let has_integral_key map =
    Option.exists (extract_key_and_value_types map) ~f:(fun (key, _) ->
        match key.Typ.desc with Tint _ -> true | _ -> false )


  let forget_lookup_witness_dsl ({FuncArg.arg_payload} as map) =
    let open PulseModelsDSL.Syntax in
    if has_integral_key map then
      let* no_witness = int 0 in
      store_field ~ref:arg_payload lookup_present_field no_witness
    else ret ()


  let set_lookup_witness_dsl arg_payload key_value =
    let open PulseModelsDSL.Syntax in
    match key_value with
    | None ->
        ret ()
    | Some (value, _) ->
        let* present = int 1 in
        store_field ~ref:arg_payload lookup_key_field value
        @@> store_field ~ref:arg_payload lookup_present_field present


  let return_value_reference_dsl ({FuncArg.arg_payload} as map) : unit PulseModelsDSL.model_monad =
    let open PulseModelsDSL.Syntax in
    Option.value_map (extract_key_and_value_types map) ~default:(ret ()) ~f:(fun (key_t, value_t) ->
        let* pair = access Read arg_payload pair_access in
        let* value_ref = access Read pair (pair_second_access key_t value_t) in
        assign_ret value_ref )


  let return_value_reference desc map =
    let open PulseModelsDSL.Syntax in
    start_named_model desc @@ fun () -> return_value_reference_dsl map


  let return_it_dsl arg_payload it : unit PulseModelsDSL.model_monad =
    let open PulseModelsDSL.Syntax in
    let* pair = access Read arg_payload pair_access in
    store ~ref:it pair


  let return_it desc arg_payload it =
    let open PulseModelsDSL.Syntax in
    start_named_model desc @@ fun () -> return_it_dsl arg_payload it


  (* A number of map functions return pair<iterator, bool>. *)
  (* Make an iterator the first field of a pair and return it. *)
  let return_it_pair_first_dsl map_payload FuncArg.{arg_payload; typ} :
      unit PulseModelsDSL.model_monad =
    let open PulseModelsDSL.Syntax in
    let* it = fresh () in
    let* pair = access Read map_payload pair_access in
    store ~ref:it pair
    @@> write_field ~ref:arg_payload (Fieldname.make (unwrap_pointer_to_struct_type typ) "first") it


  let reset_backing_fields_dsl ({FuncArg.arg_payload} as map) : unit PulseModelsDSL.model_monad =
    let open PulseModelsDSL.Syntax in
    Option.value_map (extract_key_and_value_types map) ~default:(ret ()) ~f:(fun (key_t, value_t) ->
        let* first = fresh () in
        let* second = fresh () in
        let* pair = fresh () in
        write_field ~ref:pair (pair_first_field key_t value_t) first
        @@> write_field ~ref:pair (pair_second_field key_t value_t) second
        @@> write_field ~ref:arg_payload pair_field pair )


  let update_last_dsl arg_payload key_payload : unit PulseModelsDSL.model_monad =
    let open PulseModelsDSL.Syntax in
    AddressAttributes.add_one (fst arg_payload) (LastLookup (fst key_payload)) |> exec_command


  let key_value_dsl map key_payload =
    let open PulseModelsDSL.Syntax in
    match extract_key_and_value_types map with
    | Some ({Typ.desc= Tint _}, _) ->
        let* value = load key_payload in
        let* constant = as_constant_q value in
        let key =
          match constant with
          | Some q when Z.equal (Q.den q) Z.one ->
              Attribute.KnownInteger (IntLit.of_big_int (Q.num q))
          | _ ->
              Attribute.SymbolicInteger (fst value)
        in
        ret (Some (value, key))
    | _ ->
        ret None


  let set_last_value_dsl ~known_present arg_payload key_value =
    let open PulseModelsDSL.Syntax in
    match key_value with
    | None ->
        ret ()
    | Some (_, key) ->
        AddressAttributes.add_one (fst arg_payload)
          (Attribute.LastLookupValue {key; first_key= Some key; known_present})
        |> exec_command


  let forget_key_membership_dsl ({FuncArg.arg_payload} as map) =
    let open PulseModelsDSL.Syntax in
    exec_command (AddressAttributes.forget_last_lookup_presence (fst arg_payload))
    @@> forget_lookup_witness_dsl map


  let update_last ~desc ({FuncArg.arg_payload} as map) key_payload =
    let open PulseModelsDSL.Syntax in
    start_named_model desc
    @@ fun () ->
    let* key_value = key_value_dsl map key_payload in
    (* Queries do not prove membership; retain their existing same-source handling. *)
    set_last_value_dsl ~known_present:false arg_payload key_value
    @@> update_last_dsl arg_payload key_payload


  let check_last_dsl arg_payload key_payload key_value : bool PulseModelsDSL.model_monad =
    let open PulseModelsDSL.Syntax in
    let* last_lookup = AddressAttributes.get_last_lookup (fst arg_payload) |> exec_pure_operation in
    let* snapshot =
      exec_pure_operation (fun astate ->
          AddressAttributes.find_opt `Post (fst arg_payload) astate
          |> Option.bind ~f:Attributes.get_last_lookup_value )
    in
    let same_source =
      Option.value_map last_lookup ~default:false ~f:(AbstractValue.equal (fst key_payload))
    in
    match (last_lookup, key_value, snapshot) with
    | Some _, Some (value, _), Some (key, first_key, known_present) ->
        let* equal, equal_first =
          exec_pure_operation (fun astate ->
              let phi = astate.AbductiveDomain.path_condition in
              let matches (key : Attribute.lookup_key) =
                match key with
                | KnownInteger old ->
                    Option.exists
                      (Formula.as_constant_q phi (fst value))
                      ~f:(fun current -> Q.equal current (Q.of_bigint (IntLit.to_big_int old)))
                | SymbolicInteger old ->
                    let repr = Formula.get_var_repr phi in
                    AbstractValue.equal (repr old) (repr (fst value))
                    || Option.exists
                         (Option.both (Formula.as_constant_q phi old)
                            (Formula.as_constant_q phi (fst value)) )
                         ~f:(fun (left, right) -> Q.equal left right)
              in
              (matches key, Option.value_map first_key ~default:true ~f:matches) )
        in
        ret ((equal && known_present) || (same_source && equal_first))
    | _ ->
        ret same_source


  let constructor map_t classname map args =
    let open PulseModelsDSL.Syntax in
    let desc = Format.asprintf "%a::%s" Invalidation.pp_map_type map_t classname in
    start_named_model desc
    @@ fun () ->
    let* {callee_procname} = get_data in
    let* () =
      match args with
      | source :: _ ->
          let same_map_type =
            Option.exists
              (Option.both
                 (struct_name_of_type map.FuncArg.typ)
                 (struct_name_of_type source.FuncArg.typ) )
              ~f:(fun (left, right) -> Typ.Name.equal left right)
          in
          let may_move =
            List.nth (IRAttributes.load_formal_types callee_procname) 1
            |> Option.value_map ~default:true ~f:Typ.is_rvalue_reference
          in
          (* A copy leaves its source's witness intact. Missing signature information must not
             justify keeping a witness for a possibly moved source. *)
          if same_map_type && may_move then forget_key_membership_dsl source else ret ()
      | _ ->
          ret ()
    in
    reset_backing_fields_dsl map @@> forget_key_membership_dsl map


  let invalidate_references_dsl map_t map_f ({FuncArg.arg_payload} as map) :
      unit PulseModelsDSL.model_monad =
    let open PulseModelsDSL.Syntax in
    Option.value_map (extract_key_and_value_types map) ~default:(ret ()) ~f:(fun (key_t, value_t) ->
        let cause = Invalidation.CppMap (map_t, map_f) in
        let* pair = access NoAccess arg_payload pair_access in
        invalidate_access cause arg_payload pair_access
        @@> invalidate_access cause pair (pair_first_access key_t value_t)
        @@> invalidate_access cause pair (pair_second_access key_t value_t)
        @@> reset_backing_fields_dsl map )


  let invalidate_references map_t map_f map =
    let open PulseModelsDSL.Syntax in
    let desc =
      Format.asprintf "%a::%a" Invalidation.pp_map_type map_t Invalidation.pp_map_function map_f
    in
    start_named_model desc
    @@ fun () ->
    let* () =
      match (map_f : Invalidation.map_function) with
      | Clear | OperatorEqual ->
          forget_key_membership_dsl map
      | _ ->
          ret ()
    in
    invalidate_references_dsl map_t map_f map


  let operator_bracket desc map_t ({FuncArg.arg_payload} as map) key_payload =
    let open PulseModelsDSL.Syntax in
    start_named_model desc
    @@ fun () ->
    let* key_value = key_value_dsl map key_payload in
    let* double_lookup = check_last_dsl arg_payload key_payload key_value in
    let* () =
      match key_value with
      | None ->
          if double_lookup then ret () else invalidate_references_dsl map_t OperatorBracket map
      | Some (key, _) ->
          (* These ordinary heap reads become input conditions in a helper's summary. Keep
             the old backing cells in the stable case, including aliases held by its caller. *)
          let* previous = load_access ~no_access:true arg_payload (FieldAccess lookup_key_field) in
          let* present =
            load_access ~no_access:true arg_payload (FieldAccess lookup_present_field)
          in
          let unmatched =
            if double_lookup then
              (* The legacy same-source shortcut can follow a query or a destructive mutation.
                 Retain that local behavior, but do not turn it into new input evidence. *)
              forget_lookup_witness_dsl map
            else
              invalidate_references_dsl map_t OperatorBracket map
              @@> set_lookup_witness_dsl arg_payload key_value
          in
          disj
            [ prune_eq_int present IntLit.one @@> prune_eq previous key
              @@> set_lookup_witness_dsl arg_payload key_value
            ; prune_ne_int present IntLit.one @@> unmatched
            ; prune_eq_int present IntLit.one @@> prune_ne previous key @@> unmatched ]
    in
    update_last_dsl arg_payload key_payload
    @@> set_last_value_dsl ~known_present:true arg_payload key_value
    @@> return_value_reference_dsl map


  let emplace_hint desc map_t map_f ({FuncArg.arg_payload} as map) args =
    let open PulseModelsDSL.Syntax in
    start_named_model desc
    @@ fun () ->
    (* We expect the last argument to be the returned iterator. *)
    (* This will only throw if SIL intermediate representation changes. *)
    let it = (List.last_exn args).FuncArg.arg_payload in
    invalidate_references_dsl map_t map_f map @@> return_it_dsl arg_payload it


  let emplace desc map_t map_f ({FuncArg.arg_payload} as map) args =
    let open PulseModelsDSL.Syntax in
    start_named_model desc
    @@ fun () ->
    let last_arg = List.last_exn args in
    invalidate_references_dsl map_t map_f map @@> return_it_pair_first_dsl arg_payload last_arg


  let try_emplace ~hinted map_t map_f map args =
    (* The hinted version returns an iterator, other overloads a pair<iterator, bool>. *)
    let desc =
      Format.asprintf "%a::%a" Invalidation.pp_map_type map_t Invalidation.pp_map_function map_f
    in
    if hinted then emplace_hint desc map_t map_f map args else emplace desc map_t map_f map args


  let insert ~hinted map_t map_f map return_arg = try_emplace ~hinted map_t map_f map [return_arg]

  let find map_t ({FuncArg.arg_payload} as map) key_payload it =
    let open PulseModelsDSL.Syntax in
    let desc = Format.asprintf "%a::find" Invalidation.pp_map_type map_t in
    start_named_model desc
    @@ fun () ->
    let* key_value = key_value_dsl map key_payload in
    update_last_dsl arg_payload key_payload
    @@> set_last_value_dsl ~known_present:false arg_payload key_value
    @@> return_it_dsl arg_payload it


  let swap map_t ({FuncArg.arg_payload} as map) ({FuncArg.arg_payload= other_payload} as other_map)
      =
    let open PulseModelsDSL.Syntax in
    let desc = Format.asprintf "%a::swap" Invalidation.pp_map_type map_t in
    start_named_model desc
    @@ fun () ->
    let* witnesses =
      if has_integral_key map && has_integral_key other_map then
        let* key = load_access ~no_access:true arg_payload (FieldAccess lookup_key_field) in
        let* present = load_access ~no_access:true arg_payload (FieldAccess lookup_present_field) in
        let* other_key = load_access ~no_access:true other_payload (FieldAccess lookup_key_field) in
        let* other_present =
          load_access ~no_access:true other_payload (FieldAccess lookup_present_field)
        in
        ret (Some (key, present, other_key, other_present))
      else ret None
    in
    let* arg_pair = access Read arg_payload pair_access in
    let* other_pair = access Read other_payload pair_access in
    write_field ~ref:arg_payload pair_field other_pair
    @@> write_field ~ref:other_payload pair_field arg_pair
    @@> forget_key_membership_dsl map
    @@> forget_key_membership_dsl other_map
    @@> Option.value_map witnesses ~default:(ret ())
          ~f:(fun (key, present, other_key, other_present) ->
            store_field ~ref:arg_payload lookup_key_field other_key
            @@> store_field ~ref:arg_payload lookup_present_field other_present
            @@> store_field ~ref:other_payload lookup_key_field key
            @@> store_field ~ref:other_payload lookup_present_field present )


  let iterator_star desc it =
    let open PulseModelsDSL.Syntax in
    start_named_model desc
    @@ fun () ->
    let* pair = load it in
    assign_ret pair


  let iterator_copy desc it other =
    let open PulseModelsDSL.Syntax in
    start_named_model desc
    @@ fun () ->
    let* pair = load other in
    store ~ref:it pair
    @@> lift_to_monad (lift_model (ForwardIterator.copy_position ~src:other ~dst:it))
end

let get_cpp_matchers =
  let cpp_separator_regex = Str.regexp_string "::" in
  fun config ~model ->
    let open ProcnameDispatcher.Call in
    List.filter_map
      ~f:(fun m ->
        match Str.split cpp_separator_regex m with
        | [] ->
            None
        | first :: rest ->
            Some (List.fold rest ~f:( &:: ) ~init:(-first) &--> model m) )
      config


let abort_matchers : matcher list =
  get_cpp_matchers ~model:(fun _ -> Basic.early_exit) Config.pulse_model_abort
  |> List.map ~f:(ProcnameDispatcher.Call.map_matcher ~f:lift_model)


module Pair = struct
  let make_pair type1 type2 value1 value2 (return_param, _) : model_no_non_disj =
    let make_pair_field =
      Fieldname.make
        (Typ.CppClass
           { name= QualifiedCppName.of_qual_string "std::pair"
           ; template_spec_info= Template {mangled= None; args= [TType type1; TType type2]}
           ; is_union= false } )
    in
    let first = make_pair_field "first" in
    let second = make_pair_field "second" in
    let desc = "std::make_pair()" in
    fun {path; location} astate ->
      let hist = Hist.single_call path location desc in
      let<**> astate, value1 = Basic.deep_copy path location ~value:value1 ~desc astate in
      let<**> astate, value2 = Basic.deep_copy path location ~value:value2 ~desc astate in
      let<*> astate =
        PulseOperations.write_field path location ~ref:(return_param, hist) first ~obj:value1 astate
      in
      let<+> astate =
        PulseOperations.write_field path location ~ref:(return_param, hist) second ~obj:value2
          astate
      in
      astate
end

module Thrift = struct
  let field_ref ~name tgt src : model =
    let open PulseModelsDSL.Syntax in
    start_named_model ("apache::thrift::" ^ name) @@ fun () -> store ~ref:tgt src


  let field_ref_star ~name this : model =
    let open PulseModelsDSL.Syntax in
    start_named_model ("apache::thrift::" ^ name ^ "::operator*")
    @@ fun () ->
    let* inner = load this in
    assign_ret inner


  let field_ref_arrow ~name this : model =
    let open PulseModelsDSL.Syntax in
    start_named_model ("apache::thrift::" ^ name ^ "::operator->")
    @@ fun () ->
    let* inner = load this in
    assign_ret inner
end

let folly_co_yield_co_error : model =
  let open PulseModelsDSL.Syntax in
  start_named_model "folly::coro::detail::*::yield_value(folly::coro::co_error)" @@ fun () -> throw


(* Run as an unknown call first: calls that cannot change the memory they read return the same
   value, so checking one of them checks the others. *)
let nullable_model args : model =
  let open PulseModelsDSL.Syntax in
  start_model
  @@ fun () ->
  L.d_printfln ~color:Orange "model for matching nullable functions" ;
  let* {analysis_data= {tenv}; callee_procname; path; location; ret= ret_id, _} = get_data in
  let* () = lift_to_monad (lift_model (PulseModelsImport.Basic.skipped_known_call args)) in
  let has_return_param =
    Option.exists (IRAttributes.load callee_procname) ~f:(fun attrs ->
        attrs.ProcAttributes.has_added_return_param )
  in
  match (List.last args, has_return_param) with
  | Some return_param, true -> (
      let desc = Procname.to_string callee_procname in
      match PulseModelsSmartPointers.nullable_return tenv return_param ~desc with
      | Some model ->
          lift_to_monad (lift_model model)
      | None ->
          ret () )
  | _ ->
      let* ((v, _) as res) = read (Exp.Var ret_id) in
      disj
        [ (let* hist = add_model_call ValueHistory.epoch in
           let null_deref = Invalidation.ConstantDereference IntLit.zero in
           let res =
             ( v
             , ValueHistory.sequence
                 (ValueHistory.Invalidated (null_deref, location, path.timestamp))
                 hist )
           in
           let* () = and_eq_int res IntLit.zero in
           let* () =
             PulseOperations.invalidate path UntraceableAccess location null_deref res
             |> exec_command
           in
           assign_ret res )
        ; prune_positive res ]


let matchers : matcher list =
  let open ProcnameDispatcher.Call in
  [ +BuiltinDecl.(match_builtin __delete) <>$ capt_arg $--> delete
  ; +BuiltinDecl.(match_builtin __delete_array) <>$ capt_arg $--> delete_array ]


let map_matchers =
  let open ProcnameDispatcher.Call in
  let value_it_matcher = -"folly" <>:: "f14" <>:: "detail" <>:: "ValueContainerIterator" in
  let vector_it_matcher = -"folly" <>:: "f14" <>:: "detail" <>:: "VectorContainerIterator" in
  let basic_string_matchers =
    let char_ptr_typ = Typ.mk (Tptr (Typ.mk (Tint IChar), Pk_pointer)) in
    let view_method method_name =
      -"std"
      &::+ (fun (_, procname) name ->
      String.equal
        (String.lsplit2 name ~on:'<' |> Option.value_map ~default:name ~f:fst)
        "basic_string_view"
      && BasicStringView.supports procname )
      &:: method_name
    in
    [ -"std" &:: "basic_string" &:: "basic_string" $ capt_arg_payload
      $+ capt_arg_payload_of_prim_typ char_ptr_typ
      $+...$--> BasicString.constructor_from_constant ~desc:"std::basic_string::basic_string()"
    ; -"std" &:: "basic_string" &:: "basic_string" $ capt_arg_payload $+ capt_arg_payload
      $+ any_arg_of_prim_typ (Typ.mk (Tint IChar))
      $+...$--> BasicString.fill_constructor ~desc:"std::basic_string::basic_string()"
    ; -"std" &:: "basic_string" &:: "basic_string" $ capt_arg_payload $+ capt_arg_payload
      $--> BasicString.copy_constructor ~desc:"std::basic_string::basic_string()"
    ; -"std" &:: "basic_string" &:: "basic_string" $ capt_arg_payload
      $--> BasicString.default_constructor ~desc:"std::basic_string::basic_string()"
    ; -"std" &:: "basic_string" &:: "operator_basic_string_view" $ capt_arg_payload
      $+ capt_arg_payload
      $--> BasicStringView.of_string ~desc:"std::basic_string::operator_basic_string_view()"
    ; -"std" &:: "basic_string" &:: "data" <>$ capt_arg_payload
      $--> BasicString.data ~desc:"std::basic_string::data()"
    ; -"std" &:: "basic_string" &:: "c_str" <>$ capt_arg_payload
      $--> BasicString.data ~desc:"std::basic_string::c_str()"
    ; view_method "basic_string_view" $ capt_arg_payload
      $--> BasicStringView.default_constructor ~desc:"std::basic_string_view::basic_string_view()"
    ; -"std" &:: "basic_string_view" &:: "basic_string_view" $ capt_arg_payload
      $+ capt_arg_payload_of_prim_typ char_ptr_typ
      $--> BasicStringView.constructor_from_char_ptr
             ~desc:"std::basic_string_view::basic_string_view()"
    ; -"std" &:: "basic_string_view" &:: "basic_string_view" $ capt_arg_payload
      $+ capt_arg_payload_of_prim_typ char_ptr_typ
      $+ capt_arg
      $--> BasicStringView.constructor_from_char_ptr_and_size
             ~desc:"std::basic_string_view::basic_string_view()"
    ; -"std" &:: "basic_string_view" &:: "basic_string_view" $ capt_arg_payload
      $+ capt_arg_payload_of_typ (-"std" &:: "basic_string_view")
      $--> BasicStringView.copy_constructor ~desc:"std::basic_string_view::basic_string_view()"
    ; -"std" &:: "basic_string_view" &:: "operator=" <>$ capt_arg_payload
      $+ capt_arg_payload_of_typ (-"std" &:: "basic_string_view")
      $--> BasicStringView.assign ~desc:"std::basic_string_view::operator=()"
    ; -"std" &:: "basic_string_view" &:: "data" <>$ capt_arg_payload
      $--> BasicStringView.data ~desc:"std::basic_string_view::data()"
    ; view_method "size" <>$ capt_arg_payload
      $--> BasicStringView.length ~desc:"std::basic_string_view::size()"
    ; view_method "length" <>$ capt_arg_payload
      $--> BasicStringView.length ~desc:"std::basic_string_view::length()"
    ; view_method "empty" <>$ capt_arg_payload $--> BasicStringView.empty
    ; view_method "operator[]" <>$ capt_arg_payload $+ capt_arg_payload $--> BasicStringView.address
    ; view_method "remove_prefix" <>$ capt_arg_payload $+ capt_arg_payload
      $--> BasicStringView.remove_affix ~desc:"std::basic_string_view::remove_prefix()"
             ~moves_data:true
    ; view_method "remove_suffix" <>$ capt_arg_payload $+ capt_arg_payload
      $--> BasicStringView.remove_affix ~desc:"std::basic_string_view::remove_suffix()"
             ~moves_data:false
    ; view_method "substr" <>$ capt_arg_payload $+ capt_arg_payload $+ capt_arg_payload
      $+ capt_arg_payload $--> BasicStringView.substr
    ; -"std" &:: "basic_string" &:: "begin" <>$ capt_arg_payload $+ capt_arg_payload
      $--> BasicString.begin_ ~desc:"std::basic_string::begin()"
    ; -"std" &:: "basic_string" &:: "end" <>$ capt_arg_payload $+ capt_arg_payload
      $--> BasicString.end_ ~desc:"std::basic_string::end()"
    ; -"std" &:: "basic_string" &:: "empty" <>$ capt_arg_payload $--> BasicString.empty
    ; -"std" &:: "basic_string" &:: "length" <>$ capt_arg_payload $--> BasicString.length
    ; -"std" &:: "basic_string" &:: "~basic_string" <>$ capt_arg_payload $--> BasicString.destructor
    ]
    @ List.map Invalidation.all_std_string_functions ~f:(fun string_f ->
        -"std" &:: "basic_string"
        &:: Invalidation.std_string_method_name string_f
        $ capt_arg $++$--> BasicString.mutator string_f |> with_non_disj )
  in
  let folly_matchers =
    List.concat_map
      [ ("F14ValueMap", Invalidation.FollyF14Value, [value_it_matcher])
      ; ("F14VectorMap", Invalidation.FollyF14Vector, [vector_it_matcher])
      ; ("F14FastMap", Invalidation.FollyF14Fast, [value_it_matcher; vector_it_matcher]) ]
      ~f:(fun (map_s, map_t, it_matchers) ->
        [ -"folly" <>:: map_s &:: map_s
          $ capt_arg_of_typ (-"folly" <>:: map_s)
          $++$--> GenericMapCollection.constructor map_t map_s
        ; -"folly" <>:: "f14" <>:: "detail" <>:: "F14BasicMap" &:: "operator="
          <>$ capt_arg_of_typ (-"folly" <>:: map_s)
          $+...$--> GenericMapCollection.invalidate_references map_t OperatorEqual
        ; -"folly" <>:: "f14" <>:: "detail" <>:: "F14BasicMap" &:: "clear"
          <>$ capt_arg_of_typ (-"folly" <>:: map_s)
          $--> GenericMapCollection.invalidate_references map_t Clear
        ; -"folly" <>:: "f14" <>:: "detail" <>:: "F14BasicMap" &:: "rehash"
          <>$ capt_arg_of_typ (-"folly" <>:: map_s)
          $+...$--> GenericMapCollection.invalidate_references map_t Rehash
        ; -"folly" <>:: "f14" <>:: "detail" <>:: "F14BasicMap" &:: "reserve"
          <>$ capt_arg_of_typ (-"folly" <>:: map_s)
          $+...$--> GenericMapCollection.invalidate_references map_t Reserve
        ; -"folly" <>:: "f14" <>:: "detail" <>:: "F14BasicMap" &:: "at"
          $ capt_arg_of_typ (-"folly" <>:: map_s)
          $+...$--> GenericMapCollection.return_value_reference
                      (Format.asprintf "folly::%s::at" map_s)
        ; -"folly" <>:: "f14" <>:: "detail" <>:: "F14BasicMap" &:: "emplace_hint"
          $ capt_arg_of_typ (-"folly" <>:: map_s)
          $++$--> GenericMapCollection.emplace_hint "folly::F14FastMap::emplace_hint" map_t
                    EmplaceHint
        ; -"folly" <>:: "f14" <>:: "detail" <>:: "F14BasicMap" &:: "emplace"
          $ capt_arg_of_typ (-"folly" <>:: map_s)
          $++$--> GenericMapCollection.emplace "folly::F14FastMap::emplace" map_t Emplace
        ; -"folly" <>:: "f14" <>:: "detail" <>:: "F14BasicMap" &:: "try_emplace_token"
          $ capt_arg_of_typ (-"folly" <>:: map_s)
          $++$--> GenericMapCollection.emplace "folly::F14FastMap::try_emplace_token" map_t
                    TryEmplaceToken
        ; -"folly" <>:: "f14" <>:: "detail" <>:: "F14BasicMap" &:: "operator[]"
          $ capt_arg_of_typ (-"folly" <>:: map_s)
          $+ capt_arg_payload
          $+...$--> GenericMapCollection.operator_bracket "folly::F14FastMap::operator[]" map_t
        ; -"folly" <>:: "f14" <>:: "detail" <>:: "F14BasicMap" &:: "find"
          $ capt_arg_of_typ (-"folly" <>:: map_s)
          $+ capt_arg_payload $+ capt_arg_payload $--> GenericMapCollection.find map_t
        ; -"folly" <>:: "f14" <>:: "detail" <>:: "F14BasicMap" &:: "begin"
          <>$ capt_arg_payload_of_typ (-"folly" <>:: map_s)
          $+ capt_arg_payload
          $--> GenericMapCollection.return_it (Format.asprintf "folly::%s::begin" map_s)
        ; -"folly" <>:: "f14" <>:: "detail" <>:: "F14BasicMap" &:: "cbegin"
          <>$ capt_arg_payload_of_typ (-"folly" <>:: map_s)
          $+ capt_arg_payload
          $--> GenericMapCollection.return_it (Format.asprintf "folly::%s::cbegin" map_s)
        ; -"folly" <>:: "f14" <>:: "detail" <>:: "F14BasicMap" &:: "contains"
          <>$ capt_arg_of_typ (-"folly" <>:: map_s)
          $+ capt_arg_payload
          $--> GenericMapCollection.update_last ~desc:"folly::f14::detail::F14BasicMap::contains"
        ; -"folly" <>:: "f14" <>:: "detail" <>:: "F14BasicMap" &:: "count"
          <>$ capt_arg_of_typ (-"folly" <>:: map_s)
          $+ capt_arg_payload
          $--> GenericMapCollection.update_last ~desc:"folly::f14::detail::F14BasicMap::count"
        ; -"folly" <>:: map_s &:: "swap"
          <>$ capt_arg_of_typ (-"folly" <>:: map_s)
          $+ capt_arg_of_typ (-"folly" <>:: map_s)
          $--> GenericMapCollection.swap map_t
          (* Order matters for the next matchers in this list. *)
          (* insert_or_assign:
              1. Two arguments only: non-hinted case.
              2. Three arguments with the first being a token: non-hinted case.
              3. Else: hinted case.
          *)
        ; -"folly" <>:: "f14" <>:: "detail" <>:: "F14BasicMap" &:: "insert_or_assign"
          $ capt_arg_of_typ (-"folly" <>:: map_s)
          $+ any_arg $+ any_arg $+ capt_arg
          $--> GenericMapCollection.insert ~hinted:false map_t InsertOrAssign
        ; -"folly" <>:: "f14" <>:: "detail" <>:: "F14BasicMap" &:: "insert_or_assign"
          $ capt_arg_of_typ (-"folly" <>:: map_s)
          $+ any_arg_of_typ (-"folly" <>:: "F14HashToken")
          $+ any_arg $+ any_arg $+ capt_arg
          $--> GenericMapCollection.insert ~hinted:false map_t InsertOrAssign
        ; -"folly" <>:: "f14" <>:: "detail" <>:: "F14BasicMap" &:: "insert_or_assign"
          $ capt_arg_of_typ (-"folly" <>:: map_s)
          $+ any_arg $+ any_arg $+ any_arg $+ capt_arg
          $--> GenericMapCollection.insert ~hinted:true map_t InsertOrAssign
          (* try_emplace:
              1. First argument is an iterator: hinted case.
              2. Else: non-hinted case.
          *)
        ; -"folly" <>:: "f14" <>:: "detail" <>:: "F14BasicMap" &:: "try_emplace"
          $ capt_arg_of_typ (-"folly" <>:: map_s)
          $+ any_arg_of_typ_exists it_matchers
          $++$--> GenericMapCollection.try_emplace ~hinted:true map_t TryEmplace
        ; -"folly" <>:: "f14" <>:: "detail" <>:: "F14BasicMap" &:: "try_emplace"
          $ capt_arg_of_typ (-"folly" <>:: map_s)
          $++$--> GenericMapCollection.try_emplace ~hinted:false map_t TryEmplace
          (* insert:
              1. One argument and it's std::initializer_list: return void.
              2. Three arguments (including SIL return): return iterator.
              3. Two arguments (including SIL return matching std::pair type):
                   return pair<iterator, bool>.
              4. Else if two arguments: return void.
          *)
        ; -"folly" <>:: "f14" <>:: "detail" <>:: "F14BasicMap" &:: "insert"
          $ capt_arg_of_typ (-"folly" <>:: map_s)
          $+ any_arg_of_typ (-"std" &:: "initializer_list")
          $--> GenericMapCollection.invalidate_references map_t Insert
        ; -"folly" <>:: "f14" <>:: "detail" <>:: "F14BasicMap" &:: "insert"
          $ capt_arg_of_typ (-"folly" <>:: map_s)
          $+ any_arg $+ any_arg $+ capt_arg
          $--> GenericMapCollection.insert ~hinted:true map_t Insert
        ; -"folly" <>:: "f14" <>:: "detail" <>:: "F14BasicMap" &:: "insert"
          $ capt_arg_of_typ (-"folly" <>:: map_s)
          $+ any_arg
          $+ capt_arg_of_typ (-"std" &:: "pair")
          $--> GenericMapCollection.insert ~hinted:false map_t Insert
        ; -"folly" <>:: "f14" <>:: "detail" <>:: "F14BasicMap" &:: "insert"
          $ capt_arg_of_typ (-"folly" <>:: map_s)
          $+ any_arg $+ any_arg
          $--> GenericMapCollection.invalidate_references map_t Insert ] )
  in
  let folly_iterator_matchers =
    List.concat_map ["ValueContainerIterator"; "VectorContainerIterator"] ~f:(fun it ->
        let copy_method method_name (_, procname) name =
          String.equal name method_name && ForwardIterator.is_copy_method procname
        in
        [ -"folly" <>:: "f14" <>:: "detail" <>:: it &::+ copy_method it <>$ capt_arg_payload
          $+ capt_arg_payload_of_typ (-"folly" <>:: "f14" <>:: "detail" <>:: it)
          $--> GenericMapCollection.iterator_copy
                 (Format.asprintf "folly::f14::detail::%s::%s" it it)
        ; -"folly" <>:: "f14" <>:: "detail" <>:: it &::+ copy_method "operator="
          <>$ capt_arg_payload
          $+ capt_arg_payload_of_typ (-"folly" <>:: "f14" <>:: "detail" <>:: it)
          $--> GenericMapCollection.iterator_copy
                 (Format.asprintf "folly::f14::detail::%s::operator=" it)
        ; -"folly" <>:: "f14" <>:: "detail" <>:: it &:: "operator->" <>$ capt_arg_payload
          $--> GenericMapCollection.iterator_star
                 (Format.asprintf "folly::f14::detail::%s::operator->" it)
        ; -"folly" <>:: "f14" <>:: "detail" <>:: it &:: "operator*" <>$ capt_arg_payload
          $--> GenericMapCollection.iterator_star
                 (Format.asprintf "folly::f14::detail::%s::operator*" it)
        ; -"folly" <>:: "f14" <>:: "detail" <>:: it &:: "operator++"
          &++> Basic.unknown_call "folly::f14::detail::it::operator++"
          |> with_non_disj ] )
  in
  let folly_concurrent_hash_map_matchers =
    (* We ignore all [folly::ConcurrentHashMap] methods as of now, because the summaries from the
       actual source code are too imprecise. *)
    [ -"folly" <>:: "ConcurrentHashMap"
      &::+ (fun _ _ -> true)
      &++> Basic.unknown_call "folly::ConcurrentHashMap" ]
    |> List.map ~f:with_non_disj
  in
  let folly_coro =
    [ -"folly" &:: "coro" &:: "detail"
      &::+ (fun _ _ -> true)
      &:: "yield_value" $ any_arg
      $+ any_arg_of_typ (-"folly" &:: "coro" &:: "co_error")
      $+ any_arg $--> folly_co_yield_co_error ]
  in
  basic_string_matchers @ folly_matchers @ folly_iterator_matchers
  @ folly_concurrent_hash_map_matchers @ folly_coro


let thrift_matchers =
  let open ProcnameDispatcher.Call in
  List.concat_map Typ.thrift_field_refs ~f:(fun field_ref ->
      [ -"apache" &:: "thrift" &:: field_ref &:: field_ref $ capt_arg_payload $+ capt_arg_payload
        $+...$--> Thrift.field_ref ~name:field_ref
      ; -"apache" &:: "thrift" &:: field_ref &:: "operator*" $ capt_arg_payload
        $--> Thrift.field_ref_star ~name:field_ref
      ; -"apache" &:: "thrift" &:: field_ref &:: "operator->" <>$ capt_arg_payload
        $--> Thrift.field_ref_arrow ~name:field_ref ] )


let condition_variable_matchers =
  let open ProcnameDispatcher.Call in
  let wait ?(returns_pred_value = true) cv meth =
    ConditionVariable.wait_with_predicate ~returns_pred_value
      ~desc:(Printf.sprintf "std::%s::%s()" cv meth)
  in
  let any = "condition_variable_any" in
  let stop_token_matchers =
    [ -"std" &:: any &:: "wait" $ any_arg $+ capt_arg_payload $+ any_arg $+ capt_arg
      $--> wait any "wait"
    ; -"std" &:: any &:: "wait_for" $ any_arg $+ capt_arg_payload $+ any_arg $+ any_arg $+ capt_arg
      $--> wait any "wait_for"
    ; -"std" &:: any &:: "wait_until" $ any_arg $+ capt_arg_payload $+ any_arg $+ any_arg
      $+ capt_arg $--> wait any "wait_until" ]
  in
  List.concat_map ["condition_variable"; any] ~f:(fun cv ->
      [ -"std" &:: cv &:: "wait" $ any_arg $+ capt_arg_payload $+ capt_arg
        $--> wait cv "wait" ~returns_pred_value:false
      ; -"std" &:: cv &:: "wait_for" $ any_arg $+ capt_arg_payload $+ any_arg $+ capt_arg
        $--> wait cv "wait_for"
      ; -"std" &:: cv &:: "wait_until" $ any_arg $+ capt_arg_payload $+ any_arg $+ capt_arg
        $--> wait cv "wait_until" ] )
  @ stop_token_matchers


(* A FATAL [android::base::LogMessage]'s destructor aborts via a function-pointer
   aborter Pulse can't follow, so model the FATAL construction as non-returning to
   prune the failing branch of [CHECK(x)]. [FATAL] is the last [LogSeverity]
   enumerator (6); [FATAL_WITHOUT_ABORT] (5) must not match. *)
let log_message_fatal severity : model =
  let open PulseModelsDSL.Syntax in
  let fatal = IntLit.of_int 6 in
  start_model
  @@ fun () ->
  disj [prune_eq_int severity fatal @@> lift_to_monad Basic.early_exit; prune_ne_int severity fatal]


let simple_matchers =
  let open ProcnameDispatcher.Call in
  let match_nullable_fn (_tenv, proc_name) _ =
    Option.exists Config.pulse_model_return_nullable ~f:(fun r ->
        let s = Procname.to_string proc_name in
        Str.string_match r s 0 )
  in
  map_matchers @ thrift_matchers @ condition_variable_matchers
  @ [ -"android" &:: "base" &:: "LogMessage" &:: "LogMessage" $ any_arg $+ any_arg $+ any_arg
      $+ capt_arg_payload $+...$--> log_message_fatal
    ; +BuiltinDecl.(match_builtin __builtin_add_overflow)
      <>$ capt_arg_payload $+ capt_arg_payload $+ capt_arg_payload $--> add_overflow
      |> with_non_disj
    ; +BuiltinDecl.(match_builtin __builtin_mul_overflow)
      <>$ capt_arg_payload $+ capt_arg_payload $+ capt_arg_payload $--> mul_overflow
      |> with_non_disj
    ; +BuiltinDecl.(match_builtin __builtin_sub_overflow)
      <>$ capt_arg_payload $+ capt_arg_payload $+ capt_arg_payload $--> sub_overflow
      |> with_non_disj
    ; +BuiltinDecl.(match_builtin __infer_skip)
      &++> Basic.unknown_call "__infer_skip" |> with_non_disj
    ; +BuiltinDecl.(match_builtin __infer_ptr_to_member_call)
      &++> Basic.unknown_call "__infer_ptr_to_member_call"
      |> with_non_disj
    ; +BuiltinDecl.(match_builtin __infer_structured_binding)
      <>$ capt_exp $+ capt_arg $--> infer_structured_binding |> with_non_disj
    ; +BuiltinDecl.(match_builtin __new) <>$ capt_exp $--> new_
    ; +BuiltinDecl.(match_builtin __new_array) <>$ capt_exp $--> new_array |> with_non_disj
    ; +BuiltinDecl.(match_builtin __placement_new) &++> placement_new
    ; -"apache" &:: "thrift"
      &::+ (fun _ s -> Typ.is_thrift_field_ref_str s)
      &:: "operator=" &--> Basic.skip |> with_non_disj
    ; -"std" &:: "basic_string" &:: "substr"
      &--> Basic.nondet ~desc:"std::basic_string::substr"
      |> with_non_disj
    ; -"std" &:: "basic_string" &:: "size" $ capt_arg_payload $--> BasicString.size
    ; -"std" &:: "basic_string" &:: "operator[]" <>$ capt_arg_payload $+ capt_arg_payload
      $--> BasicString.address ~desc:"std::basic_string::operator[]"
      |> with_non_disj
    ; -"std" &:: "function" &:: "function" $ capt_arg_payload $+ capt_arg
      $--> Function.assign ~desc:"std::function::function"
      |> with_non_disj
    ; -"folly" &:: "Function" &:: "Function" $ capt_arg_payload $+ capt_arg
      $--> Function.assign ~desc:"folly::Function::Function"
      |> with_non_disj
    ; -"folly" &:: "Function" &:: "operator=" $ capt_arg_payload $+ capt_arg
      $--> Function.assign ~desc:"folly::Function::operator="
      |> with_non_disj
    ; -"folly" &:: "Function" &:: "operator_bool"
      &--> Basic.nondet ~desc:"folly::Function::operator_bool()"
      |> with_non_disj
    ; -"folly" &:: "Function" &:: "~Function" &--> Basic.skip |> with_non_disj
    ; -"std" &:: "function" &:: "operator()" $ capt_arg
      $++$--> Function.operator_call ~deref_lambda_ptr:false
    ; -"std" &:: "call_once" $ capt_arg $+ capt_arg $--> Function.call_once
    ; -"folly" &:: "detail" &:: "function" &:: "FunctionTraits" &:: "operator()" $ capt_arg
      $++$--> Function.operator_call ~deref_lambda_ptr:true
    ; -"std" &:: "function" &:: "operator=" $ capt_arg_payload $+ capt_arg
      $--> Function.assign ~desc:"std::function::operator="
      |> with_non_disj
    ; -"std" &:: "atomic" &:: "atomic" <>$ capt_arg_payload $+ capt_arg_payload
      $--> AtomicInteger.constructor |> with_non_disj
    ; -"std" &:: "__atomic_base" &:: "fetch_add" <>$ capt_arg_payload $+ capt_arg_payload
      $+ capt_arg_payload $--> AtomicInteger.fetch_add |> with_non_disj
    ; -"std" &:: "__atomic_base" &:: "fetch_sub" <>$ capt_arg_payload $+ capt_arg_payload
      $+ capt_arg_payload $--> AtomicInteger.fetch_sub |> with_non_disj
    ; -"std" &:: "__atomic_base" &:: "exchange" <>$ capt_arg_payload $+ capt_arg_payload
      $+ capt_arg_payload $--> AtomicInteger.exchange |> with_non_disj
    ; -"std" &:: "__atomic_base" &:: "load" <>$ capt_arg_payload $+? capt_arg_payload
      $--> AtomicInteger.load |> with_non_disj
    ; -"std" &:: "__atomic_base" &:: "store" <>$ capt_arg_payload $+ capt_arg_payload
      $+ capt_arg_payload $--> AtomicInteger.store |> with_non_disj
    ; -"std" &:: "__atomic_base" &:: "operator++" <>$ capt_arg_payload
      $--> AtomicInteger.operator_plus_plus_pre |> with_non_disj
    ; -"std" &:: "__atomic_base" &:: "operator++" <>$ capt_arg_payload $+ capt_arg_payload
      $--> AtomicInteger.operator_plus_plus_post |> with_non_disj
    ; -"std" &:: "__atomic_base" &:: "operator--" <>$ capt_arg_payload
      $--> AtomicInteger.operator_minus_minus_pre |> with_non_disj
    ; -"std" &:: "__atomic_base" &:: "operator--" <>$ capt_arg_payload $+ capt_arg_payload
      $--> AtomicInteger.operator_minus_minus_post |> with_non_disj
    ; -"std" &:: "__atomic_base"
      &::+ (fun _ name -> String.is_prefix ~prefix:"operator_" name)
      <>$ capt_arg_payload $+? capt_arg_payload $--> AtomicInteger.operator_t |> with_non_disj
    ; -"std" &:: "as_const" $ capt_arg_payload
      $--> Basic.id_first_arg ~desc:"std::as_const()"
      |> with_non_disj
    ; -"std" &:: "make_move_iterator" $ capt_arg_payload $+...$--> Std.make_move_iterator
      |> with_non_disj
    ; -"std" &:: "make_pair" < capt_typ &+ capt_typ >$ capt_arg_payload $+ capt_arg_payload
      $+ capt_arg_payload $--> Pair.make_pair |> with_non_disj
    ; -"std" &:: "vector" &:: "vector" $ capt_arg_payload
      $--> Vector.default_constructor ~desc:"std::vector::vector()"
      |> with_non_disj
      |> observe_matcher ~rule:"std.vector.vector.1"
    ; -"std" &:: "vector" &:: "vector" <>$ capt_arg_payload
      $+ capt_arg_payload_of_typ (-"std" &:: "initializer_list")
      $+...$--> Vector.init_list_constructor ~desc:"std::vector::vector()"
      |> with_non_disj
      |> observe_matcher ~rule:"std.vector.vector.2"
    ; -"std" &:: "vector" &:: "vector" <>$ capt_arg_payload
      $+ capt_arg_payload_of_typ (-"std" &:: "vector")
      $+...$--> Vector.init_copy_constructor ~desc:"std::vector::vector()"
      |> with_non_disj
      |> observe_matcher ~rule:"std.vector.vector.3"
    ; -"std" &:: "vector" &:: "assign" $ capt_arg_payload
      $+ capt_arg_payload_of_typ (-"std" &:: "initializer_list")
      $--> Vector.assign_initializer_list |> with_non_disj
    ; -"std" &:: "vector" &:: "assign" $ capt_arg_payload $+ capt_arg $+ any_arg
      $--> Vector.assign_count |> with_non_disj
    ; -"std" &:: "vector" &:: "assign" $ capt_arg_payload
      $+...$--> Vector.invalidate_references_unknown_size Assign
      |> with_non_disj
    ; -"std" &:: "vector" &:: "at" <>$ capt_arg_payload $+ capt_arg_payload
      $--> Vector.at ~desc:"std::vector::at()"
      |> with_non_disj
    ; -"std" &:: "vector" &:: "back" <>$ capt_arg_payload
      $--> Vector.back ~desc:"std::vector::back()"
      |> with_non_disj
      |> observe_matcher ~rule:"std.vector.back.1"
    ; -"std" &:: "vector" &:: "begin" <>$ capt_arg_payload $+ capt_arg_payload
      $--> Vector.vector_begin ~desc:"std::vector::begin()"
      |> with_non_disj
      |> observe_matcher ~rule:"std.vector.begin.1"
    ; -"std" &:: "vector" &:: "cbegin" <>$ capt_arg_payload $+ capt_arg_payload
      $--> Vector.vector_begin ~desc:"std::vector::cbegin()"
      |> with_non_disj
    ; -"std" &:: "vector" &:: "end" <>$ capt_arg_payload $+ capt_arg_payload
      $--> Vector.vector_end ~desc:"std::vector::end()"
      |> with_non_disj
      |> observe_matcher ~rule:"std.vector.end.1"
    ; -"std" &:: "vector" &:: "cend" <>$ capt_arg_payload $+ capt_arg_payload
      $--> Vector.vector_end ~desc:"std::vector::cend()"
      |> with_non_disj
    ; -"std" &:: "vector" &:: "clear" <>$ capt_arg_payload $--> Vector.clear
      |> observe_matcher ~rule:"std.vector.clear.1"
    ; -"std" &:: "vector" &:: "~vector" <>$ capt_arg_payload $+...$--> Vector.destructor
      |> observe_matcher ~rule:"std.vector.destructor.1"
    ; -"std" &:: "vector" &:: "data" <>$ capt_arg_payload
      $--> Vector.data ~desc:"std::vector::data()"
      |> with_non_disj
      |> observe_matcher ~rule:"std.vector.data.1"
    ; -"std" &:: "vector" &:: "emplace" $ capt_arg_payload
      $+...$--> Vector.invalidate_references_one_more Emplace
      |> with_non_disj
    ; -"std" &:: "vector" &:: "emplace_back" $ capt_arg_payload
      $++$--> Vector.emplace_back ~desc:"std::vector::emplace_back()"
      |> with_non_disj
    ; (* the returned iterator is passed as the last argument *)
      -"std" &:: "vector" &:: "erase" <>$ capt_arg_payload $+ capt_arg_payload $+ capt_arg_payload
      $--> Vector.erase ~range:false |> with_non_disj
      |> observe_matcher ~rule:"std.vector.erase.1"
    ; -"std" &:: "vector" &:: "erase" <>$ capt_arg_payload $+ capt_arg_payload $+ any_arg
      $+ capt_arg_payload $--> Vector.erase ~range:true |> with_non_disj
      |> observe_matcher ~rule:"std.vector.erase.2"
    ; -"std" &:: "vector" &:: "front" <>$ capt_arg_payload
      $--> Vector.front ~desc:"std::vector::front()"
      |> with_non_disj
      |> observe_matcher ~rule:"std.vector.front.1"
    ; -"std" &:: "vector" &:: "insert" $ capt_arg_payload $+ any_arg
      $+ capt_arg_payload_of_typ (-"std" &:: "initializer_list")
      $+ any_arg $--> Vector.insert_initializer_list |> with_non_disj
    ; -"std" &:: "vector" &:: "insert" $ capt_arg_payload $+ any_arg $+ capt_arg $+ any_arg
      $+ any_arg $--> Vector.insert_count |> with_non_disj
    ; (* [insert(pos, value)], the returned iterator is passed as the last argument *)
      -"std" &:: "vector" &:: "insert" $ capt_arg_payload $+ capt_arg_payload $+ any_arg
      $+ capt_arg_payload $--> Vector.insert_one |> with_non_disj
    ; -"std" &:: "vector" &:: "insert" $ capt_arg_payload
      $+...$--> Vector.invalidate_references_unknown_size Insert
      |> with_non_disj
    ; -"std" &:: "vector" &:: "operator=" <>$ capt_arg_payload $+ capt_arg $+...$--> Vector.assign
      |> observe_matcher ~rule:"std.vector.assign.1"
    ; -"std" &:: "vector" &:: "operator[]" <>$ capt_arg_payload $+ capt_arg_payload
      $--> Vector.at ~desc:"std::vector::at()"
      |> with_non_disj
      |> observe_matcher ~rule:"std.vector.subscript.1"
    ; -"std" &:: "vector" &:: "shrink_to_fit" <>$ capt_arg_payload
      $--> Vector.invalidate_references ShrinkToFit
      |> with_non_disj
      |> observe_matcher ~rule:"std.vector.shrink_to_fit.1"
    ; -"std" &:: "vector" &:: "push_back" <>$ capt_arg_payload $+ capt_arg
      $--> Vector.push_back_cpp ~vector_f:PushBack ~desc:"std::vector::push_back()"
      |> observe_matcher ~rule:"std.vector.push_back.1"
    ; -"std" &:: "vector" &:: "pop_back" <>$ capt_arg_payload
      $+...$--> Vector.pop_back ~desc:"std::vector::pop_back()"
      |> observe_matcher ~rule:"std.vector.pop_back.1"
    ; -"std" &:: "vector" &:: "empty" <>$ capt_arg_payload
      $--> GenericArrayBackedCollection.empty ~desc:"std::vector::is_empty()"
      |> with_non_disj
    ; -"std" &:: "vector" &:: "reserve" <>$ capt_arg_payload $+ capt_arg $+...$--> Vector.reserve
      |> observe_matcher ~rule:"std.vector.reserve.1"
    ; -"std" &:: "vector" &:: "resize" <>$ capt_arg_payload $+ capt_arg $+...$--> Vector.resize
      |> with_non_disj
      |> observe_matcher ~rule:"std.vector.resize.1"
    ; -"std" &:: "vector" &:: "size" $ capt_arg_payload
      $--> GenericArrayBackedCollection.size ~desc:"std::vector::size()"
      |> with_non_disj
      |> observe_matcher ~rule:"std.vector.size.1"
      (* [std::span]: the constructors are function templates, so they are matched with any
         template arguments, and the order of the two-argument ones matters. *)
    ; -"std" &:: "span" &:: "span" $ capt_arg_payload
      $+ capt_arg_payload_of_typ_exists [-"std" &:: "span"; -"std" &:: "vector"]
      $--> Span.constructor_from_collection ~desc:"std::span::span()"
    ; -"std" &:: "span" &:: "span" $ capt_arg_payload $+ capt_arg_payload $+ capt_arg
      $--> Span.constructor_from_iterator ~desc:"std::span::span()"
    ; -"std" &:: "span" &:: "span" $ capt_arg_payload $+ capt_arg
      $--> Span.constructor_from_array ~desc:"std::span::span()"
    ; -"std" &:: "span" &:: "span" $ capt_arg_payload
      $--> Span.default_constructor ~desc:"std::span::span()"
    ; -"std" &:: "span" &:: "operator=" <>$ capt_arg_payload
      $+ capt_arg_payload_of_typ (-"std" &:: "span")
      $--> Span.assign ~desc:"std::span::operator=()"
    ; (* a span does not own what it views *)
      -"std" &:: "span" &:: "~span" <>$ any_arg $--> Basic.skip |> with_non_disj
    ; -"std" &:: "span" &:: "data" <>$ capt_arg_payload $--> Span.data ~desc:"std::span::data()"
    ; -"std" &:: "span" &:: "size" <>$ capt_arg_payload
      $--> GenericArrayBackedCollection.size ~desc:"std::span::size()"
      |> with_non_disj
    ; -"std" &:: "span" &:: "empty" <>$ capt_arg_payload
      $--> GenericArrayBackedCollection.empty ~desc:"std::span::empty()"
      |> with_non_disj
    ; -"std" &:: "span" &:: "operator[]" <>$ capt_arg_payload $+ capt_arg_payload
      $--> Vector.at ~desc:"std::span::operator[]()"
      |> with_non_disj
    ; -"std" &:: "span" &:: "front" <>$ capt_arg_payload
      $--> Vector.front ~desc:"std::span::front()"
      |> with_non_disj
    ; -"std" &:: "span" &:: "back" <>$ capt_arg_payload
      $--> Vector.back ~desc:"std::span::back()"
      |> with_non_disj
    ; -"std" &:: "span" &:: "begin" <>$ capt_arg_payload $+ capt_arg_payload
      $--> Vector.vector_begin ~desc:"std::span::begin()"
      |> with_non_disj
    ; -"std" &:: "span" &:: "end" <>$ capt_arg_payload $+ capt_arg_payload
      $--> Vector.vector_end ~desc:"std::span::end()"
      |> with_non_disj
    ; -"std" &:: "span" &:: "first" <>$ capt_arg_payload $+ capt_arg_payload $+ capt_arg_payload
      $--> Span.first ~desc:"std::span::first()"
    ; -"std" &:: "span" &:: "last" <>$ capt_arg_payload $+ capt_arg_payload $+ capt_arg_payload
      $--> Span.last ~desc:"std::span::last()"
    ; -"std" &:: "span" &:: "subspan" <>$ capt_arg_payload $+ capt_arg_payload $+ capt_arg_payload
      $+ capt_arg_payload
      $--> Span.subspan ~desc:"std::span::subspan()"
    ; -"std"
      &::+ PointerIterator.is_function_on_pointers ["distance"]
      $ capt_arg_payload $+ capt_arg_payload
      $--> PointerIterator.distance ~desc:"std::distance"
    ; -"std" &:: "distance"
      $ capt_arg_payload_of_typ_exists
          [-"std" &:: "__wrap_iter"; -"__gnu_cxx" &:: "__normal_iterator"]
      $+ capt_arg_payload
      $--> GenericArrayBackedCollection.Iterator.distance ~desc:"std::distance"
      |> with_non_disj
    ; -"std"
      &::+ PointerIterator.is_function_on_pointers ["next"]
      $ capt_arg_payload $+? capt_arg_payload
      $--> PointerIterator.offset ~desc:"std::next" PlusPI
    ; -"std"
      &::+ PointerIterator.is_function_on_pointers ["prev"]
      $ capt_arg_payload $+? capt_arg_payload
      $--> PointerIterator.offset ~desc:"std::prev" MinusPI
    ; -"std"
      &::+ PointerIterator.is_function_on_pointers ["advance"]
      $ capt_arg_payload $+ capt_arg_payload
      $--> PointerIterator.advance ~desc:"std::advance"
    ; +BuiltinDecl.(match_builtin __infer_initializer_list)
      <>$ capt_arg $+...$--> InitializerList.make
    ; -"std" &:: "initializer_list" &:: "initializer_list" <>$ capt_arg_payload
      $+ capt_arg_payload_of_typ (-"std" &:: "initializer_list")
      $--> InitializerList.copy_constructor ~desc:"std::initializer_list::initializer_list(copy)"
      |> with_non_disj
    ; -"std" &:: "initializer_list" &:: "operator=" <>$ capt_arg_payload
      $+ capt_arg_payload_of_typ (-"std" &:: "initializer_list")
      $--> InitializerList.copy_constructor ~desc:"std::initializer_list::operator=()"
      |> with_non_disj
    ; -"std" &:: "initializer_list" &:: "initializer_list" <>$ capt_arg_payload
      $--> InitializerList.default_constructor ~desc:"std::initializer_list::initializer_list()"
    ; -"std" &:: "initializer_list" &:: "size" <>$ capt_arg_payload
      $--> InitializerList.size ~desc:"std::initializer_list::size()"
    ; -"std" &:: "initializer_list" &:: "begin" <>$ capt_arg_payload
      $--> InitializerList.begin_ ~desc:"std::initializer_list::begin()"
    ; -"std" &:: "initializer_list" &:: "end" <>$ capt_arg_payload
      $--> InitializerList.end_ ~desc:"std::initializer_list::end()"
    ; -"std" &:: "distance" $ capt_arg $+ capt_arg
      $--> ForwardIterator.distance ~desc:"std::distance"
      |> with_non_disj
    ; -"std" &:: "integral_constant" < any_typ &+ capt_int
      >::+ (fun _ name -> String.is_prefix ~prefix:"operator_" name)
      <>--> Basic.return_int ~desc:"std::integral_constant"
      |> with_non_disj
    ; (* consider that all fbstrings are small strings to avoid false positives due to manual
         ref-counting *)
      -"folly" &:: "fbstring_core" &:: "category"
      &--> Basic.return_int Int64.zero ~desc:"folly::fbstring_core::category"
      |> with_non_disj
    ; -"folly" &:: "DelayedDestruction" &:: "destroy"
      &++> Basic.unknown_call "folly::DelayedDestruction::destroy is modelled as skip"
      |> with_non_disj
    ; -"folly" &:: "SocketAddress" &:: "~SocketAddress"
      &++> Basic.unknown_call "folly::SocketAddress's destructor is modelled as skip"
      |> with_non_disj
    ; -"folly" &:: "detail" &:: "SingletonHolder" &:: "get"
      &++> Basic.unknown_call "folly::detail::SingletonHolder::get"
      |> with_non_disj
    ; -"folly" &:: "detail" &:: "SingletonHolder" &:: "get_weak"
      &++> Basic.unknown_call "folly::detail::SingletonHolder::get_weak"
      |> with_non_disj
    ; -"folly" &:: "detail" &:: "SingletonHolder" &:: "try_get"
      &++> Basic.unknown_call "folly::detail::SingletonHolder::try_get"
      |> with_non_disj
    ; -"folly" &:: "detail" &:: "SingletonHolder" &:: "try_get_fast"
      &++> Basic.unknown_call "folly::detail::SingletonHolder::try_get_fast"
      |> with_non_disj
    ; -"folly" &:: "Try" &:: "emplace"
      &++> Basic.unknown_call "folly::Try::emplace"
      |> with_non_disj
    ; -"folly" &:: "detail" &:: "TryBase" &:: "TryBase"
      &++> Basic.unknown_call "folly::detail::TryBase::TryBase"
      |> with_non_disj
    ; -"folly" &:: "Expected" &:: "hasValue"
      &--> Basic.nondet ~desc:"folly::Expected::hasValue()"
      |> with_non_disj
    ; -"folly" &:: "Expected" &:: "hasError"
      &--> Basic.nondet ~desc:"folly::Expected::hasError()"
      |> with_non_disj
    ; -"folly" &:: "Expected" &:: "value"
      &++> Basic.unknown_call "folly::Expected::value"
      |> with_non_disj
    ; -"folly" &:: "Expected" &:: "operator*"
      &++> Basic.unknown_call "folly::Expected::operator*"
      |> with_non_disj
    ; -"folly" &:: "Expected" &:: "operator_bool"
      &--> Basic.nondet ~desc:"folly::Expected::operator_bool()"
      |> with_non_disj
    ; -"folly" &:: "Expected" &:: "Expected"
      &++> Basic.unknown_call "folly::Expected::Expected"
      |> with_non_disj
    ; -"folly" &:: "Expected" &:: "~Expected" &--> Basic.skip |> with_non_disj
    ; -"folly" &:: "expected_detail" &:: "ExpectedUnion" &:: "ExpectedUnion"
      &++> Basic.unknown_call "folly::expected_detail::ExpectedUnion"
      |> with_non_disj
    ; -"folly" &:: "expected_detail" &:: "ExpectedStorage" &:: "ExpectedStorage"
      &++> Basic.unknown_call "folly::expected_detail::ExpectedStorage"
      |> with_non_disj
    ; +match_nullable_fn &::.*++> nullable_model ]


let matchers =
  matchers
  @ List.map simple_matchers
      ~f:(ProcnameDispatcher.Call.contramap_arg_payload ~f:ValueOrigin.addr_hist)
