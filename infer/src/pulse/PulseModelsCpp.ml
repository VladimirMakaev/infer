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

module BasicString = struct
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
  let data_field = Fieldname.make PulseOperations.pulse_model_type "__infer_model_string_view_data"

  let store_data this_hist (data, data_hist) : unit PulseModelsDSL.model_monad =
    let open PulseModelsDSL.Syntax in
    let* data_hist = add_model_call data_hist in
    store_field ~ref:this_hist data_field (data, data_hist)


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

module Vector = struct
  let reallocate_internal_array path trace vector vector_f location astate =
    let* astate, array_address =
      GenericArrayBackedCollection.eval path NoAccess location vector astate
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
    (* missing a more precise model for std::initializer_list *)
    let<**> astate =
      GenericArrayBackedCollection.assign_size_constant path location this ~constant:IntLit.zero
        ~desc astate
    in
    let<+> astate = shallow_copy_init_list path location this init_list ~desc astate in
    astate


  let is_move_constructor callee_procname =
    List.nth (IRAttributes.load_formal_types callee_procname) 1
    |> Option.exists ~f:Typ.is_rvalue_reference


  let init_copy_constructor this init_vector ~desc : model_no_non_disj =
   fun {path; location; callee_procname} astate ->
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
    let depth_max = if is_move_constructor callee_procname then 0 else 1 in
    let<*> astate, (copy, _) =
      PulseOperations.deep_copy ~depth_max path location init_list astate
    in
    let<+> astate =
      PulseOperations.write_deref_field path location ~ref:this GenericArrayBackedCollection.field
        ~obj:(copy, Hist.add_call path location desc (snd init_list))
        astate
    in
    astate


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
        ~index:index_zero astate
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
    let<+> astate =
      PulseOperations.write_field path location ~ref:iter
        GenericArrayBackedCollection.Iterator.internal_pointer ~obj:pointer_val astate
    in
    astate


  let reserve vector : model_no_non_disj =
   fun {path; location} astate ->
    let hist = Hist.single_call path location "std::vector::reserve()" in
    let<+> astate =
      reallocate_internal_array path hist vector Reserve location astate
      >>| AddressAttributes.std_vector_reserve (fst vector)
    in
    astate


  let pop_back vector ~desc : model_no_non_disj =
   fun {path; location} astate ->
    let<++> astate = GenericArrayBackedCollection.decrease_size path location vector ~desc astate in
    astate


  let reallocate_unless_reserved path location vector vector_f ~desc astate =
    if AddressAttributes.is_std_vector_reserved (fst vector) astate then
      (* assume that growing the vector is ok after one called [reserve] on the same vector (a
         perfect analysis would also make sure we don't exceed the reserved size) *)
      Ok astate
    else
      match vector_f with
      | None ->
          Ok astate
      | Some vector_f ->
          (* simulate a re-allocation of the underlying array every time the vector grows *)
          reallocate_internal_array path
            (Hist.single_call path location desc)
            vector vector_f location astate


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


  let push_back_cpp vector value ~vector_f ~desc =
    push_back_common ~value vector ~vector_f:(Some vector_f) ~desc


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


  let resize vector size : model_no_non_disj =
   fun {path; location} astate ->
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
      if may_grow then reallocate_unless_reserved path location vector (Some Resize) ~desc astate
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
    let provably_lt astate lhs =
      PulseArithmetic.prune_binop ~negated:false Ge (AbstractValueOperand lhs)
        (AbstractValueOperand position) astate
      |> SatUnsat.sat |> Option.is_none
    in
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
    let<*> astate = erase_from path location event vector position astate in
    (* the returned iterator is at the erased position, unless that position is the one of
       [begin()] and was invalidated above as the element at index 0 *)
    if
      AddressAttributes.find_opt `Post position astate
      |> Option.exists ~f:(fun attrs -> Attributes.get_invalid attrs |> Option.is_some)
    then vector_begin ~desc vector iter model_data astate
    else
      let<+> astate =
        GenericArrayBackedCollection.Iterator.point_at path location event ~collection:vector ~iter
          ~position astate
      in
      astate
end

module GenericMapCollection = struct
  let pair_field = Fieldname.make PulseOperations.pulse_model_type "__infer_map_pair"

  let pair_access = Access.FieldAccess pair_field

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


  let update_last ~desc arg_payload key_payload =
    let open PulseModelsDSL.Syntax in
    start_named_model desc @@ fun () -> update_last_dsl arg_payload key_payload


  let check_and_update_last_dsl arg_payload key_payload : bool PulseModelsDSL.model_monad =
    let open PulseModelsDSL.Syntax in
    let* last_lookup = AddressAttributes.get_last_lookup (fst arg_payload) |> exec_pure_operation in
    update_last_dsl arg_payload key_payload
    @@> ret (Option.value_map last_lookup ~default:false ~f:(AbstractValue.equal (fst key_payload)))


  let constructor map_t classname map =
    let open PulseModelsDSL.Syntax in
    let desc = Format.asprintf "%a::%s" Invalidation.pp_map_type map_t classname in
    start_named_model desc @@ fun () -> reset_backing_fields_dsl map


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
    start_named_model desc @@ fun () -> invalidate_references_dsl map_t map_f map


  let operator_bracket desc map_t ({FuncArg.arg_payload} as map) key_payload =
    let open PulseModelsDSL.Syntax in
    start_named_model desc
    @@ fun () ->
    let* double_lookup = check_and_update_last_dsl arg_payload key_payload in
    let* () =
      if double_lookup then ret () else invalidate_references_dsl map_t OperatorBracket map
    in
    return_value_reference_dsl map


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

  let find map_t arg_payload key_payload it =
    let open PulseModelsDSL.Syntax in
    let desc = Format.asprintf "%a::find" Invalidation.pp_map_type map_t in
    start_named_model desc
    @@ fun () -> update_last_dsl arg_payload key_payload @@> return_it_dsl arg_payload it


  let swap map_t arg_payload other_payload =
    let open PulseModelsDSL.Syntax in
    let desc = Format.asprintf "%a::swap" Invalidation.pp_map_type map_t in
    start_named_model desc
    @@ fun () ->
    let* arg_pair = access Read arg_payload pair_access in
    let* other_pair = access Read other_payload pair_access in
    write_field ~ref:arg_payload pair_field other_pair
    @@> write_field ~ref:other_payload pair_field arg_pair


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
    [ -"std" &:: "basic_string" &:: "basic_string" $ capt_arg_payload
      $+ capt_arg_payload_of_prim_typ char_ptr_typ
      $+...$--> BasicString.constructor_from_constant ~desc:"std::basic_string::basic_string()"
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
          $+...$--> GenericMapCollection.constructor map_t map_s
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
          $ capt_arg_payload_of_typ (-"folly" <>:: map_s)
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
          <>$ capt_arg_payload_of_typ (-"folly" <>:: map_s)
          $+ capt_arg_payload
          $--> GenericMapCollection.update_last ~desc:"folly::f14::detail::F14BasicMap::contains"
        ; -"folly" <>:: "f14" <>:: "detail" <>:: "F14BasicMap" &:: "count"
          <>$ capt_arg_payload_of_typ (-"folly" <>:: map_s)
          $+ capt_arg_payload
          $--> GenericMapCollection.update_last ~desc:"folly::f14::detail::F14BasicMap::count"
        ; -"folly" <>:: map_s &:: "swap"
          <>$ capt_arg_payload_of_typ (-"folly" <>:: map_s)
          $+ capt_arg_payload_of_typ (-"folly" <>:: map_s)
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
        [ -"folly" <>:: "f14" <>:: "detail" <>:: it &:: it <>$ capt_arg_payload $+ capt_arg_payload
          $--> GenericMapCollection.iterator_copy
                 (Format.asprintf "folly::f14::detail::%s::%s" it it)
        ; -"folly" <>:: "f14" <>:: "detail" <>:: it &:: "operator=" <>$ capt_arg_payload
          $+ capt_arg_payload
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
    ; -"std" &:: "basic_string" &:: "size"
      &--> Basic.nondet ~desc:"std::basic_string::size"
      |> with_non_disj
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
    ; -"std" &:: "make_move_iterator" $ capt_arg_payload $+...$--> Std.make_move_iterator
      |> with_non_disj
    ; -"std" &:: "make_pair" < capt_typ &+ capt_typ >$ capt_arg_payload $+ capt_arg_payload
      $+ capt_arg_payload $--> Pair.make_pair |> with_non_disj
    ; -"std" &:: "vector" &:: "vector" $ capt_arg_payload
      $--> GenericArrayBackedCollection.default_constructor ~desc:"std::vector::vector()"
      |> with_non_disj
    ; -"std" &:: "vector" &:: "vector" <>$ capt_arg_payload
      $+ capt_arg_payload_of_typ (-"std" &:: "initializer_list")
      $+...$--> Vector.init_list_constructor ~desc:"std::vector::vector()"
      |> with_non_disj
    ; -"std" &:: "vector" &:: "vector" <>$ capt_arg_payload
      $+ capt_arg_payload_of_typ (-"std" &:: "vector")
      $+...$--> Vector.init_copy_constructor ~desc:"std::vector::vector()"
      |> with_non_disj
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
    ; -"std" &:: "vector" &:: "begin" <>$ capt_arg_payload $+ capt_arg_payload
      $--> Vector.vector_begin ~desc:"std::vector::begin()"
      |> with_non_disj
    ; -"std" &:: "vector" &:: "cbegin" <>$ capt_arg_payload $+ capt_arg_payload
      $--> Vector.vector_begin ~desc:"std::vector::cbegin()"
      |> with_non_disj
    ; -"std" &:: "vector" &:: "end" <>$ capt_arg_payload $+ capt_arg_payload
      $--> Vector.vector_end ~desc:"std::vector::end()"
      |> with_non_disj
    ; -"std" &:: "vector" &:: "cend" <>$ capt_arg_payload $+ capt_arg_payload
      $--> Vector.vector_end ~desc:"std::vector::cend()"
      |> with_non_disj
    ; -"std" &:: "vector" &:: "clear" <>$ capt_arg_payload
      $--> Vector.invalidate_references Clear
      |> with_non_disj
    ; -"std" &:: "vector" &:: "data" <>$ capt_arg_payload
      $--> Vector.data ~desc:"std::vector::data()"
      |> with_non_disj
    ; -"std" &:: "vector" &:: "emplace" $ capt_arg_payload
      $+...$--> Vector.invalidate_references_one_more Emplace
      |> with_non_disj
    ; -"std" &:: "vector" &:: "emplace_back" $ capt_arg_payload
      $++$--> Vector.emplace_back ~desc:"std::vector::emplace_back()"
      |> with_non_disj
    ; (* the returned iterator is passed as the last argument *)
      -"std" &:: "vector" &:: "erase" <>$ capt_arg_payload $+ capt_arg_payload $+ capt_arg_payload
      $--> Vector.erase ~range:false |> with_non_disj
    ; -"std" &:: "vector" &:: "erase" <>$ capt_arg_payload $+ capt_arg_payload $+ any_arg
      $+ capt_arg_payload $--> Vector.erase ~range:true |> with_non_disj
    ; -"std" &:: "vector" &:: "front" <>$ capt_arg_payload
      $--> Vector.front ~desc:"std::vector::front()"
      |> with_non_disj
    ; -"std" &:: "vector" &:: "insert" $ capt_arg_payload $+ any_arg
      $+ any_arg_of_typ (-"std" &:: "initializer_list")
      $+ any_arg
      $--> Vector.invalidate_references_unknown_size Insert
      |> with_non_disj
    ; (* [insert(pos, value)], the returned iterator is passed as the last argument *)
      -"std" &:: "vector" &:: "insert" $ capt_arg_payload $+ any_arg $+ any_arg $+ any_arg
      $--> Vector.invalidate_references_one_more Insert
      |> with_non_disj
    ; -"std" &:: "vector" &:: "insert" $ capt_arg_payload
      $+...$--> Vector.invalidate_references_unknown_size Insert
      |> with_non_disj
    ; -"std" &:: "vector" &:: "operator=" <>$ capt_arg_payload
      $+...$--> Vector.invalidate_references_with_ret Assign
      |> with_non_disj
    ; -"std" &:: "vector" &:: "operator[]" <>$ capt_arg_payload $+ capt_arg_payload
      $--> Vector.at ~desc:"std::vector::at()"
      |> with_non_disj
    ; -"std" &:: "vector" &:: "shrink_to_fit" <>$ capt_arg_payload
      $--> Vector.invalidate_references ShrinkToFit
      |> with_non_disj
    ; -"std" &:: "vector" &:: "push_back" <>$ capt_arg_payload $+ capt_arg
      $--> Vector.push_back_cpp ~vector_f:PushBack ~desc:"std::vector::push_back()"
      |> with_non_disj
    ; -"std" &:: "vector" &:: "pop_back" <>$ capt_arg_payload
      $+...$--> Vector.pop_back ~desc:"std::vector::pop_back()"
      |> with_non_disj
    ; -"std" &:: "vector" &:: "empty" <>$ capt_arg_payload
      $--> GenericArrayBackedCollection.empty ~desc:"std::vector::is_empty()"
      |> with_non_disj
    ; -"std" &:: "vector" &:: "reserve" <>$ capt_arg_payload $+...$--> Vector.reserve
      |> with_non_disj
    ; -"std" &:: "vector" &:: "resize" <>$ capt_arg_payload $+ capt_arg_payload
      $+...$--> Vector.resize |> with_non_disj
    ; -"std" &:: "vector" &:: "size" $ capt_arg_payload
      $--> GenericArrayBackedCollection.size ~desc:"std::vector::size()"
      |> with_non_disj
    ; -"std" &:: "distance" &--> Basic.nondet ~desc:"std::distance" |> with_non_disj
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
