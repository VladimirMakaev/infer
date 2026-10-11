(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

open! IStd
module IRAttributes = Attributes
open PulseBasicInterface
open PulseOperationResult.Import
module Domain = PulseAbductiveDomain
module Path = PulsePathContext
module J = Yojson.Basic.Util

exception AdapterError of string

let adapter_error reason = raise (AdapterError reason)

type slot = {value: AbstractValue.t; typ: Typ.t; exp: Exp.t}

type state = {heap: Domain.t; slots: slot IString.Map.t; errors: string list}

let int_typ = Typ.mk (Tint IInt)

let ptr_typ = Typ.mk_ptr int_typ

let history = ValueHistory.epoch

let location = Location.dummy

let operation_location = ref location

let class_name name args =
  Typ.Name.Cpp.from_qual_name
    (Template {mangled= None; args= List.map args ~f:(fun typ -> Typ.TType typ)})
    ~is_union:false
    (QualifiedCppName.of_list ["std"; name])


let vector_name = class_name "vector" [int_typ]

let deleter_typ = Typ.mk (Tstruct (class_name "default_delete" [int_typ]))

let unique_name = class_name "unique_ptr" [int_typ; deleter_typ]

let iterator_typ = Typ.mk_ptr (Typ.mk (Tstruct (class_name "__wrap_iter" [ptr_typ])))

let string key json = J.member key json |> J.to_string

let opt_string key json = J.member key json |> J.to_string_option

let integer key json = J.member key json |> J.to_int

let get name state = IString.Map.find_opt name state.slots |> Option.value_exn

let bind name slot state = {state with slots= IString.Map.add name slot state.slots}

let value_result (result : 'a PulseAccessResult.t) =
  match result with
  | Ok value | Recoverable (value, _) ->
      value
  | FatalError _ ->
      adapter_error "model adapter setup operation failed"


let sat_result = function
  | Sat result ->
      value_result result
  | Unsat _ ->
      adapter_error "model adapter setup arithmetic is unsatisfiable"


let fresh ~name ~typ ~constant state =
  let value = AbstractValue.mk_fresh () in
  let heap = PulseArithmetic.and_eq_int value (IntLit.of_int constant) state.heap |> sat_result in
  bind name {value; typ; exp= Exp.Const (Cint (IntLit.of_int constant))} {state with heap}


let object_ ~name ~typ state =
  let value = AbstractValue.mk_fresh () in
  let heap = PulseArithmetic.and_positive value state.heap |> sat_result in
  bind name {value; typ; exp= Exp.Var (Ident.create_fresh Ident.knormal)} {state with heap}


let arg slot =
  {FuncArg.arg_payload= ValueOrigin.unknown (slot.value, history); exp= slot.exp; typ= slot.typ}


let caller = Procname.from_string_c_fun "model_contract_driver"

let proc_attrs = ProcAttributes.default (SourceFile.invalid "__model_contract__") caller

let proc_desc = Procdesc.from_proc_attributes proc_attrs

let unsupported_call _data _path _ret _exp _args _loc _flags heap non_disj _callee =
  ignore (heap, non_disj) ;
  adapter_error "unsupported_direct_callback"


let analysis_data : PulseSummary.t InterproceduralAnalysis.t =
  { proc_desc
  ; tenv= Tenv.create ()
  ; err_log= Errlog.empty ()
  ; analyze_dependency= (fun ?specialization:_ _ -> Error AnalysisResult.UnknownProcedure)
  ; add_errlog= (fun _ _ -> ()) }


let name_of_method class_typ method_name parameters =
  let kind =
    if String.equal method_name "vector" || String.equal method_name "unique_ptr" then
      Procname.ObjC_Cpp.CPPConstructor None
    else if String.is_prefix ~prefix:"~" method_name then Procname.ObjC_Cpp.CPPDestructor None
    else Procname.ObjC_Cpp.CPPMethod None
  in
  Procname.ObjC_Cpp
    (Procname.ObjC_Cpp.make class_typ method_name kind NoTemplate
       (List.map parameters ~f:(fun (s : slot) -> Procname.Parameter.of_typ s.typ)) )


let call ~object_name ~method_name ~parameters ~result_name ~return_typ state =
  let location = !operation_location in
  let object_slot = get object_name state in
  let class_typ =
    match object_slot.typ.desc with
    | Tptr ({desc= Tstruct name}, _) ->
        name
    | _ ->
        adapter_error "not a model object"
  in
  let callee = name_of_method class_typ method_name parameters in
  let args = List.map (object_slot :: parameters) ~f:arg in
  let ret = Ident.create_fresh Ident.knormal in
  match PulseModels.dispatch analysis_data.tenv callee args with
  | None ->
      PulseTestObservability.call ~caller ~callee:(Some callee) ~loc:location ~route:"unknown"
        ~reason:"direct_model_unavailable" ;
      [{state with errors= ("unsupported_model:" ^ method_name) :: state.errors}]
  | Some model ->
      PulseTestObservability.call ~caller ~callee:(Some callee) ~loc:location ~route:"model"
        ~reason:"direct_model_dispatch" ;
      let data : PulseModelsImport.model_data =
        { analysis_data
        ; dispatch_call_eval_args= unsupported_call
        ; path= Path.initial
        ; callee_procname= callee
        ; location
        ; ret= (ret, return_typ)
        ; call_flags= CallFlags.default }
      in
      let outcomes, _ = model data state.heap PulseNonDisjunctiveDomain.bottom in
      List.map outcomes ~f:(fun outcome ->
          let result, errors =
            match outcome with
            | Ok result ->
                (Some result, state.errors)
            | Recoverable (result, _) ->
                (Some result, "recoverable_model_error" :: state.errors)
            | FatalError _ ->
                (None, "model_error" :: state.errors)
          in
          match result with
          | Some (PulseExecutionDomain.ContinueProgram heap) ->
              let state = {state with heap; errors} in
              Option.value_map result_name ~default:state ~f:(fun name ->
                  match PulseOperations.read_id ret heap with
                  | Some (value, _) ->
                      bind name
                        {value; typ= return_typ; exp= Exp.Var (Ident.create_fresh Ident.knormal)}
                        state
                  | None ->
                      {state with errors= "missing_return" :: errors} )
          | Some (PulseExecutionDomain.ExceptionRaised _) ->
              {state with errors= "exception" :: errors}
          | Some (PulseExecutionDomain.Stopped _) | None ->
              {state with errors= "stopped" :: errors} )


let operation json state =
  let location = !operation_location in
  let op = string "op" json in
  if List.mem state.errors "stopped" ~equal:String.equal then [state]
  else if String.equal op "allocate" then
    let name = string "name" json in
    let state = object_ ~name ~typ:ptr_typ state in
    let address = (get name state).value in
    let heap = PulseOperations.allocate Attribute.CppNew location address state.heap in
    let stored = AbstractValue.mk_fresh () in
    let heap = PulseArithmetic.and_eq_int stored IntLit.zero heap |> sat_result in
    let heap =
      PulseOperations.write_deref Path.initial location ~ref:(address, history)
        ~obj:(stored, history) heap
      |> value_result
    in
    [{state with heap}]
  else if String.equal op "integer" then
    [fresh ~name:(string "name" json) ~typ:int_typ ~constant:(integer "value" json) state]
  else if String.equal op "use" then
    let slot = get (string "name" json) state in
    let address = slot.value in
    if Typ.equal slot.typ iterator_typ then
      call ~object_name:(string "name" json) ~method_name:"operator*" ~parameters:[]
        ~result_name:(Some "$iterator_element") ~return_typ:ptr_typ state
    else
      let result =
        PulseOperations.eval_access Path.initial Read location (address, history) Dereference
          state.heap
      in
      [ ( match result with
        | Ok (heap, _) ->
            {state with heap}
        | Recoverable ((heap, _), _) ->
            {state with heap; errors= "invalid_access" :: state.errors}
        | FatalError _ ->
            {state with errors= "invalid_access" :: state.errors} ) ]
  else if String.equal op "delete_raw" then
    let slot = get (string "name" json) state in
    let data : PulseModelsImport.model_data =
      { analysis_data
      ; dispatch_call_eval_args= unsupported_call
      ; path= Path.initial
      ; callee_procname= Procname.from_string_c_fun "__delete"
      ; location
      ; ret= (Ident.create_fresh Ident.knormal, Typ.mk Tvoid)
      ; call_flags= CallFlags.default }
    in
    let outcomes, _ =
      PulseModelsImport.Basic.free_or_delete `Delete Invalidation.CppDelete (arg slot) data
        state.heap PulseNonDisjunctiveDomain.bottom
    in
    List.map outcomes ~f:(function
      | Ok (PulseExecutionDomain.ContinueProgram heap) ->
          {state with heap}
      | _ ->
          {state with errors= "invalid_access" :: state.errors} )
  else if String.equal op "end_alias" || String.equal op "begin_alias" then
    let object_name = string "object" json in
    let name = string "name" json in
    let state = object_ ~name ~typ:iterator_typ state in
    let method_name = if String.equal op "end_alias" then "end" else "begin" in
    call ~object_name ~method_name
      ~parameters:[get name state]
      ~result_name:None ~return_typ:(Typ.mk Tvoid) state
  else
    let object_name = string "object" json in
    let object_slot = get object_name state in
    let is_vector = Typ.equal object_slot.typ (Typ.mk_ptr (Typ.mk (Tstruct vector_name))) in
    let state =
      if String.equal op "alias" && is_vector then
        fresh ~name:"$index" ~typ:int_typ ~constant:(integer "index" json) state
      else if String.equal op "reserve" then
        fresh ~name:"$count" ~typ:int_typ ~constant:(integer "count" json) state
      else state
    in
    let method_name, params, result_name, return_typ =
      match op with
      | "construct" ->
          ( (if is_vector then "vector" else "unique_ptr")
          , Option.to_list (Option.map (opt_string "value" json) ~f:(fun name -> get name state))
          , None
          , Typ.mk Tvoid )
      | "adopt" ->
          ("reset", [get (string "value" json) state], None, Typ.mk Tvoid)
      | "alias" ->
          if is_vector then ("operator[]", [get "$index" state], opt_string "name" json, ptr_typ)
          else ("get", [], opt_string "name" json, ptr_typ)
      | "reserve" ->
          ("reserve", [get "$count" state], None, Typ.mk Tvoid)
      | "push" ->
          ("push_back", [get (string "value" json) state], None, Typ.mk Tvoid)
      | "pop" ->
          ("pop_back", [], None, Typ.mk Tvoid)
      | "clear" ->
          ("clear", [], None, Typ.mk Tvoid)
      | "shrink" ->
          ("shrink_to_fit", [], None, Typ.mk Tvoid)
      | "reset" ->
          ("reset", [], None, Typ.mk Tvoid)
      | "reset_to" ->
          ("reset", [get (string "value" json) state], None, Typ.mk Tvoid)
      | "release" ->
          ("release", [], opt_string "name" json, ptr_typ)
      | "move" ->
          ("operator=", [get (string "from" json) state], None, object_slot.typ)
      | "destroy" ->
          ((if is_vector then "~vector" else "~unique_ptr"), [], None, Typ.mk Tvoid)
      | _ ->
          adapter_error ("unknown operation: " ^ op)
    in
    call ~object_name ~method_name ~parameters:params ~result_name ~return_typ state


let invalid state value =
  Domain.AddressAttributes.find_opt `Post value state.heap
  |> Option.exists ~f:(fun attrs -> Option.is_some (Attributes.get_invalid attrs))


let snapshot state =
  let vector_facts =
    IString.Map.bindings state.slots
    |> List.filter_map ~f:(fun (name, (slot : slot)) ->
        if not (Typ.equal slot.typ (Typ.mk_ptr (Typ.mk (Tstruct vector_name)))) then None
        else
          match
            PulseModelsGenericArrayBackedCollection.to_internal_size_deref Path.initial NoAccess
              location (slot.value, history) state.heap
          with
          | FatalError _ ->
              Some (`Assoc [("object", `String name); ("size", `Null)])
          | Ok (heap, (size, _)) | Recoverable ((heap, (size, _)), _) ->
              let feasible n =
                match PulseArithmetic.and_eq_int size (IntLit.of_int n) heap with
                | Sat _ ->
                    true
                | Unsat _ ->
                    false
              in
              let values = List.init 9 ~f:Fn.id |> List.filter ~f:feasible in
              Some
                (`Assoc
                   [ ("object", `String name)
                   ; ("size_values_0_to_8", `List (List.map values ~f:(fun n -> `Int n)))
                   ; ("capacity", `Null) ] ) )
  in
  let slots =
    IString.Map.bindings state.slots
    |> List.map ~f:(fun (name, (slot : slot)) ->
        `Assoc
          [ ("name", `String name)
          ; ("value_id", `String (Format.asprintf "%a" AbstractValue.pp slot.value))
          ; ("invalid", `Bool (invalid state slot.value))
          ; ("null", `Bool (PulseArithmetic.is_known_zero state.heap slot.value)) ] )
  in
  `Assoc
    [ ("slots", `List slots)
    ; ("vectors", `List vector_facts)
    ; ("errors", `List (List.map (List.rev state.errors) ~f:(fun e -> `String e))) ]


let current_stage = ref "startup"

let run input =
  current_stage := "results_directory" ;
  ResultsDir.create_results_dir () ;
  DBWriter.start () ;
  let source = proc_attrs.ProcAttributes.translation_unit in
  let widths : IntegerWidths.t =
    {char_width= 8; short_width= 16; int_width= 32; long_width= 64; longlong_width= 64}
  in
  let cfg = Cfg.create () in
  ignore (Cfg.create_proc_desc cfg proc_attrs) ;
  current_stage := "procedure_attributes" ;
  IRAttributes.store ~proc_desc:None proc_attrs ~analysis:false ;
  current_stage := "source_metadata" ;
  SourceFiles.add source cfg (Tenv.FileLocal analysis_data.tenv) (Some widths) ;
  Language.set_language Language.Clang ;
  current_stage := "analysis_context" ;
  AnalysisGlobalState.initialize proc_desc analysis_data.tenv ;
  AnalysisState.set_remaining_disjuncts 64 ;
  let initial =
    {heap= Domain.mk_initial analysis_data.tenv proc_attrs; slots= IString.Map.empty; errors= []}
  in
  let initial =
    J.member "objects" input |> J.to_list
    |> List.fold ~init:initial ~f:(fun state obj ->
        let typ = if String.equal (string "kind" obj) "vector" then vector_name else unique_name in
        current_stage := "object:" ^ string "name" obj ;
        object_ ~name:(string "name" obj) ~typ:(Typ.mk_ptr (Typ.mk (Tstruct typ))) state )
  in
  let states, snapshots =
    J.member "operations" input |> J.to_list
    |> List.foldi ~init:([initial], []) ~f:(fun index (states, snapshots) op ->
        operation_location := {location with line= index + 1} ;
        current_stage := "operation:" ^ string "op" op ;
        let states = List.concat_map states ~f:(operation op) in
        (states, `List (List.map states ~f:snapshot) :: snapshots) )
  in
  `Assoc
    [ ("schema_version", `Int 1)
    ; ("case_id", J.member "case_id" input)
    ; ("layer", `String "actual_model_dispatch")
    ; ("context", `String "synthetic_LP64_no_CPP_CFG_or_summaries")
    ; ("snapshots", `List (List.rev snapshots))
    ; ("final_states", `List (List.map states ~f:snapshot)) ]


let () =
  let input = In_channel.input_all In_channel.stdin |> Yojson.Basic.from_string in
  let failed = ref false in
  let output =
    try run input
    with exn ->
      failed := true ;
      Yojson.Basic.Util.(
        `Assoc
          [ ("schema_version", `Int 1)
          ; ("case_id", member "case_id" input)
          ; ("status", `String "adapter_failure")
          ; ("stage", `String !current_stage)
          ; ("reason", `String (Exn.to_string exn)) ] )
  in
  Yojson.Basic.to_channel Out_channel.stdout output ;
  Out_channel.newline Out_channel.stdout ;
  Out_channel.flush Out_channel.stdout ;
  if !failed then Stdlib.exit 1
