(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

open! IStd
module AccessExpression = HilExp.AccessExpression
module F = Format
module L = Logging

type analysis_data =
  {interproc: RacerDDomain.summary InterproceduralAnalysis.t; formals: FormalMap.t}

module GuardIdentity = struct
  module AE = HilExp.AccessExpression

  type state =
    { roots: AE.t Var.Map.t
    ; aliases: (AE.t * AE.t) list
    ; guards: (AE.t * AE.t) list
    ; observed: AE.t list
    ; acquired: AE.t list
    ; released: AE.t list }

  type proof = {acquired_lock: AE.t; released_lock: AE.t; visible_lock: AE.t}

  let empty =
    {roots= Var.Map.empty; aliases= []; guards= []; observed= []; acquired= []; released= []}


  let put key value bindings =
    (key, value) :: List.filter bindings ~f:(fun (old, _) -> not (AE.equal key old))


  (* Preserve every field and pointer edge. Only inverse address/dereference operations cancel. *)
  let step (prefix : AE.t) (edge : HilExp.t option MemoryAccess.t) =
    match (edge, prefix) with
    | MemoryAccess.TakeAddress, Dereference parent ->
        Some parent
    | _ ->
        AE.add_access prefix edge


  let rec normalize ?(follow_alias = true) (state : state) fuel (exp : AE.t) : AE.t option =
    if fuel <= 0 then None
    else
      let rebuilt =
        match exp with
        | Base (var, _) -> (
          match Var.Map.find_opt var state.roots with
          | Some value when not (AE.equal exp value) ->
              normalize state (fuel - 1) value
          | _ ->
              Some exp )
        | FieldOffset (prefix, field) ->
            normalize state (fuel - 1) prefix
            |> Option.map ~f:(fun prefix -> AE.field_offset prefix field)
        | AddressOf prefix ->
            normalize state (fuel - 1) prefix
            |> Option.bind ~f:(fun prefix -> step prefix MemoryAccess.TakeAddress)
        | Dereference prefix ->
            normalize state (fuel - 1) prefix
            |> Option.bind ~f:(fun prefix -> step prefix MemoryAccess.Dereference)
        | ArrayOffset _ ->
            None
      in
      Option.bind rebuilt ~f:(fun rebuilt ->
          if not follow_alias then Some rebuilt
          else
            match List.Assoc.find state.aliases ~equal:AE.equal rebuilt with
            | Some value when not (AE.equal rebuilt value) ->
                normalize state (fuel - 1) value
            | _ ->
                Some rebuilt )


  let expression (state : state) (exp : HilExp.t) : AE.t option =
    match exp with AccessExpression path -> normalize state 64 path | _ -> None


  let record (state : state) ~acquire (exp : HilExp.t) : state option =
    expression state exp
    |> Option.map ~f:(fun lock ->
        if acquire then {state with acquired= lock :: state.acquired}
        else {state with released= lock :: state.released} )


  let rec union_path tenv (exp : AE.t) =
    let union_type (typ : Typ.t) =
      match typ.desc with
      | Tstruct (CUnion _) | Tptr ({desc= Tstruct (CUnion _)}, _) ->
          true
      | _ ->
          false
    in
    Option.exists (AE.get_typ exp tenv) ~f:union_type
    ||
    match exp with
    | FieldOffset (parent, _) | AddressOf parent | Dereference parent ->
        union_path tenv parent
    | ArrayOffset _ ->
        true
    | Base _ ->
        false


  let observe (state : state) (exp : AE.t) =
    let rec fields current (exp : AE.t) =
      match exp with
      | FieldOffset (parent, _) ->
          Option.bind (normalize ~follow_alias:false current 64 exp) ~f:(fun field ->
              fields {current with observed= field :: current.observed} parent )
      | AddressOf parent | Dereference parent ->
          fields current parent
      | Base _ ->
          Some current
      | ArrayOffset _ ->
          None
    in
    fields state exp


  let assign tenv formals (state : state) (lhs : AE.t) (rhs : HilExp.t) : state option =
    (* Constructor input symbols remain immutable. A reassignment cannot reuse their old identity. *)
    match lhs with
    | Base base when FormalMap.is_formal base formals ->
        None
    | _ when union_path tenv lhs ->
        None
    | _ -> (
      match AE.get_typ lhs tenv with
      | Some {Typ.desc= Tptr _} ->
          let target =
            match lhs with Base _ -> Some lhs | _ -> normalize ~follow_alias:false state 64 lhs
          in
          Option.bind target ~f:(fun target ->
              let written =
                (* Keep constructor disambiguation principal. *)
                match (target : AE.t) with
                | Base (var, _) ->
                    Var.Map.mem var state.roots
                | _ ->
                    Option.is_some (List.Assoc.find state.aliases ~equal:AE.equal target)
                    || List.mem state.observed target ~equal:AE.equal
              in
              if written || not (List.is_empty state.acquired && List.is_empty state.released) then
                None
              else
                expression state rhs
                |> Option.map ~f:(fun value ->
                    (* Keep constructor disambiguation principal. *)
                    match (target : AE.t) with
                    | Base (var, _) ->
                        {state with roots= Var.Map.add var value state.roots}
                    | _ ->
                        {state with aliases= put target value state.aliases} ) )
      | Some {Typ.desc= Tstruct _ | Tarray _} ->
          None
      | Some _ ->
          Some state
      | None ->
          None )


  (* Interpret captured straight-line bodies with production HIL conversion. Missing bodies,
     branch/loop paths, unknown expressions, transfers and conditional lock effects decline. *)
  let rec evaluate tenv stack (pname : Procname.t) (actuals : HilExp.t list option) (state : state)
      : state option =
    if List.length stack >= 32 || List.mem stack pname ~equal:Procname.equal then None
    else
      Procdesc.load pname
      |> Option.bind ~f:(fun proc_desc ->
          let formals = FormalMap.make (Procdesc.get_attributes proc_desc) in
          let inputs =
            match actuals with
            | None ->
                Some state
            | Some actuals ->
                List.foldi actuals ~init:(Some state) ~f:(fun index current actual ->
                    Option.bind current ~f:(fun current ->
                        Option.bind (FormalMap.get_formal_base index formals) ~f:(fun (var, _) ->
                            expression current actual
                            |> Option.map ~f:(fun value ->
                                {current with roots= Var.Map.add var value current.roots} ) ) ) )
          in
          let current = ref inputs in
          let ssa = ref Var.Map.empty in
          let resolve var = Var.Map.find_opt var !ssa in
          let rec simple_address (exp : Exp.t) =
            match exp with
            | Var _ | Lvar _ ->
                true
            | Lfield ({exp= parent}, _, _) ->
                simple_address parent
            | _ ->
                false
          in
          let inspect (instr : Sil.instr) =
            let supported =
              match instr with
              | Load {e} ->
                  simple_address e
              | Store {e1} ->
                  simple_address e1
              | Call (_, Exp.Const (Const.Cfun callee), _, _, _)
                when Procname.equal callee BuiltinDecl.__cast ->
                  false
              | _ ->
                  true
            in
            if not supported then current := None
            else
              current :=
                Option.bind !current ~f:(fun current ->
                    match
                      HilInstr.of_sil ~include_array_indexes:false ~f_resolve_id:resolve instr
                    with
                    | Bind (var, exp) ->
                        (* Keep production SSA expression shapes, including frontend-temp
                            dereferences. Reference cells cannot be rewritten after observation. *)
                        ssa := Var.Map.add var exp !ssa ;
                        observe current exp
                    | Instr (Assign (lhs, rhs, _)) ->
                        assign tenv formals current lhs rhs
                    | Instr (Call (_, Direct callee, actuals, call_flags, _)) ->
                        if
                          call_flags.CallFlags.cf_virtual || call_flags.cf_interface
                          || call_flags.cf_assign_last_arg
                        then None
                        else call tenv (pname :: stack) current callee actuals
                    | Instr (Metadata (LoopBackEdge _ | LoopEntry _))
                    | Instr (Call (_, Indirect _, _, _, _) | Assume _) ->
                        None
                    | Instr (Metadata _) ->
                        Some current )
          in
          let rec walk seen node =
            if Option.is_none !current then ()
            else if List.exists seen ~f:(Procdesc.Node.equal node) then current := None
            else (
              Instrs.iter (Procdesc.Node.get_instrs node) ~f:inspect ;
              match Procdesc.Node.get_succs node with
              | [] ->
                  ()
              | [next] ->
                  walk (node :: seen) next
              | _ ->
                  current := None )
          in
          walk [] (Procdesc.get_start_node proc_desc) ;
          !current )


  and call tenv stack (state : state) (pname : Procname.t) actuals : state option =
    match ConcurrencyModels.get_lock_effect pname actuals with
    | Lock [lock] ->
        record state ~acquire:true lock
    | Unlock [lock] ->
        record state ~acquire:false lock
    | GuardConstruct {guard; locks= [lock]; acquire_now= true} ->
        Option.bind (expression state guard) ~f:(fun guard ->
            Option.bind (expression state lock) ~f:(fun lock ->
                Some
                  {state with guards= put guard lock state.guards; acquired= lock :: state.acquired} ) )
    | GuardDestroy guard ->
        Option.bind (expression state guard) ~f:(fun guard ->
            List.Assoc.find state.guards ~equal:AE.equal guard
            |> Option.map ~f:(fun lock ->
                { state with
                  guards= List.filter state.guards ~f:(fun (old, _) -> not (AE.equal guard old))
                ; released= lock :: state.released } ) )
    | NoEffect ->
        evaluate tenv stack pname (Some actuals) state
    | _ ->
        None


  let project formals actuals (exp : AE.t) : AE.t option =
    Option.bind
      (FormalMap.get_formal_index (AE.get_base exp) formals)
      ~f:(fun index ->
        Option.bind (List.nth actuals index) ~f:(function
          | HilExp.AccessExpression actual ->
              let edges = AE.to_accesses exp |> snd in
              List.fold edges ~init:(Some actual) ~f:(fun prefix edge ->
                  Option.bind prefix ~f:(fun prefix -> step prefix edge) )
          | _ ->
              None ) )


  let prove tenv ~constructor ~destructor ?actuals () : proof option =
    Option.bind (Attributes.load constructor) ~f:(fun constructor_attrs ->
        let formals = FormalMap.make constructor_attrs in
        (* State and lock-proof records have disjoint labels. *)
        Option.bind (evaluate tenv [] constructor None empty) ~f:(fun (after_constructor : state) ->
            match (after_constructor.acquired, after_constructor.released) with
            | [acquired], [] ->
                Option.bind (FormalMap.get_formal_base 0 formals) ~f:(fun receiver ->
                    let after_constructor = {after_constructor with acquired= []; released= []} in
                    evaluate tenv [] destructor
                      (Some [HilExp.AccessExpression (AE.base receiver)])
                      after_constructor
                    (* The destructor retains constructor state and reference-field bindings. *)
                    |> Option.bind ~f:(fun (after_destructor : state) ->
                        match (after_destructor.acquired, after_destructor.released) with
                        | [], [released] -> (
                            let outside_receiver (exp : AE.t) =
                              FormalMap.get_formal_index (AE.get_base exp) formals
                              |> Option.exists ~f:(fun index -> index > 0)
                            in
                            if not (outside_receiver acquired && outside_receiver released) then
                              None
                            else
                              match actuals with
                              | None ->
                                  Some
                                    { acquired_lock= acquired
                                    ; released_lock= released
                                    ; visible_lock= acquired }
                              | Some actuals ->
                                  Option.bind (project formals actuals acquired) ~f:(fun acquired ->
                                      Option.bind (project formals actuals released)
                                        ~f:(fun released ->
                                          Option.bind (List.hd actuals) ~f:(function
                                            | HilExp.AccessExpression guard ->
                                                if
                                                  Var.equal
                                                    (fst (AE.get_base acquired))
                                                    (fst (AE.get_base guard))
                                                then None
                                                else
                                                  Some
                                                    { acquired_lock= acquired
                                                    ; released_lock= released
                                                    ; visible_lock= acquired }
                                            | _ ->
                                                None ) ) ) )
                        | _ ->
                            None ) )
            | _ ->
                None ) )
end

module TransferFunctions (CFG : ProcCfg.S) = struct
  module CFG = CFG
  module Domain = RacerDDomain

  type nonrec analysis_data = analysis_data

  let do_call_acquiring_ownership ret_access_exp astate =
    let open Domain in
    let ownership =
      OwnershipDomain.add ret_access_exp OwnershipAbstractValue.owned astate.ownership
    in
    {astate with ownership}


  let process_call_without_summary tenv ret_access_exp callee_pname actuals astate =
    let open Domain in
    if RacerDModels.is_synchronized_container_constructor tenv callee_pname actuals then
      apply_to_first_actual actuals astate ~f:(fun receiver ->
          let attribute_map = AttributeMapDomain.add receiver Synchronized astate.attribute_map in
          {astate with attribute_map} )
    else if RacerDModels.is_converter_to_synchronized_container tenv callee_pname actuals then
      let attribute_map = AttributeMapDomain.add ret_access_exp Synchronized astate.attribute_map in
      {astate with attribute_map}
    else if RacerDModels.is_box callee_pname then
      apply_to_first_actual actuals astate ~f:(fun actual_access_expr ->
          if AttributeMapDomain.is_functional astate.attribute_map actual_access_expr then
            (* TODO: check for constants, which are functional? *)
            let attribute_map =
              AttributeMapDomain.add ret_access_exp Functional astate.attribute_map
            in
            {astate with attribute_map}
          else astate )
    else do_call_acquiring_ownership ret_access_exp astate


  let process_for_unannotated_interface_call tenv formals call_flags callee_pname actuals loc astate
      =
    if RacerDModels.should_flag_interface_call tenv actuals call_flags callee_pname then
      Domain.add_unannotated_call_access formals callee_pname actuals loc astate
    else astate


  let process_for_thread_assert_effect ret_access_exp callee_pname (astate : Domain.t) =
    let open Domain in
    match ConcurrencyModels.get_thread_assert_effect callee_pname with
    | BackgroundThread ->
        {astate with threads= ThreadsDomain.AnyThread}
    | MainThread ->
        {astate with threads= ThreadsDomain.AnyThreadButSelf}
    | MainThreadIfTrue ->
        let attribute_map =
          AttributeMapDomain.add ret_access_exp Attribute.OnMainThread astate.attribute_map
        in
        {astate with attribute_map}
    | UnknownThread ->
        astate


  (* Constructor and destructor are distinct variants, neither is a [CPPMethod]. *)
  let cpp_guard_role = function
    | Procname.ObjC_Cpp {kind= CPPConstructor _; class_name} ->
        Some (`Acquire, class_name)
    | Procname.ObjC_Cpp {kind= CPPDestructor _; class_name} ->
        Some (`Release, class_name)
    | _ ->
        None


  (* A candidate guard must release the same caller-reachable lock it acquired. *)
  let paired_guard_summary ?actuals analyze_dependency tenv pname (summary : Domain.summary) =
    match cpp_guard_role pname with
    | None ->
        false
    | Some (role, class_name) ->
        summary.uses_locks && (not summary.never_returns)
        && Option.exists (Tenv.lookup tenv class_name) ~f:(fun ({methods} : Struct.t) ->
            List.exists methods ~f:(fun method_info ->
                let other_pname = Struct.name_of_tenv_method method_info in
                match cpp_guard_role other_pname with
                | Some (other_role, other_class)
                  when (not (Poly.equal role other_role)) && Typ.Name.equal class_name other_class
                  -> (
                  match analyze_dependency other_pname with
                  | Ok (other : Domain.summary) when other.uses_locks && not other.never_returns ->
                      let constructor, destructor =
                        match role with
                        | `Acquire ->
                            (summary.locks, other.locks)
                        | `Release ->
                            (other.locks, summary.locks)
                      in
                      let constructor_pname, destructor_pname =
                        match role with
                        | `Acquire ->
                            (pname, other_pname)
                        | `Release ->
                            (other_pname, pname)
                      in
                      GuardIdentity.prove tenv ~constructor:constructor_pname
                        ~destructor:destructor_pname ?actuals ()
                      |> Option.exists ~f:(fun (identity : GuardIdentity.proof) ->
                          Domain.LockDomain.is_guard_pair ~constructor ~destructor
                            ~acquired:(Some identity.acquired_lock)
                            ~released:(Some identity.released_lock)
                            ~caller_visible:(Some identity.visible_lock) )
                  | _ ->
                      false )
                | _ ->
                    false ) )


  let local_guard_object proc_desc formals = function
    | HilExp.AccessExpression ((Base _ | AddressOf (Base _)) as access_exp) ->
        let ((var, _) as base) = AccessExpression.get_base access_exp in
        (not (FormalMap.is_formal base formals || Var.is_global var))
        && Option.exists (Var.get_pvar var) ~f:(fun pvar ->
            not (Procdesc.is_captured_pvar proc_desc pvar) )
    | _ ->
        false


  let direct_receiver_call proc_desc formals callee_pname actuals =
    (* Keep constructor disambiguation principal. *)
    let rec base_only (access_exp : HilExp.AccessExpression.t) =
      match access_exp with
      | HilExp.AccessExpression.Base base ->
          Some base
      | Dereference exp | AddressOf exp ->
          base_only exp
      | FieldOffset _ | ArrayOffset _ ->
          None
    in
    match
      ( Procname.get_class_type_name (Procdesc.get_proc_name proc_desc)
      , Procname.get_class_type_name callee_pname
      , List.hd actuals )
    with
    | Some caller_class, Some callee_class, Some (HilExp.AccessExpression exp) ->
        Typ.Name.equal caller_class callee_class
        && Option.exists (base_only exp) ~f:(fun ((var, _) as base) ->
            Var.is_this var
            && Option.equal Int.equal (FormalMap.get_formal_index base formals) (Some 0) )
    | _ ->
        false


  (* A deliberately small helper grammar: one straight-line normal path, one local guard
     construction/destruction pair, no other call, and only receiver field accesses. In particular,
     a balanced backlog drain, wait loop, parser, I/O operation or cache population is not this
     grammar. Exceptional paths are not asserted to be straight-line by this shape check. *)
  let critical_guard_helper analyze_dependency tenv pname (summary : Domain.summary)
      (attrs : ProcAttributes.t) =
    let is_private_method =
      match pname with
      | Procname.ObjC_Cpp {kind= CPPMethod _} ->
          ProcAttributes.(equal_access (get_access attrs) Private)
      | _ ->
          false
    in
    let helper_formals = FormalMap.make attrs in
    let receiver_field_accesses =
      is_private_method && attrs.ProcAttributes.is_defined
      && Int.equal (FormalMap.cardinal helper_formals) 1
      && Domain.AccessDomain.fold
           (fun (snapshot : Domain.AccessSnapshot.t) all_receiver_fields ->
             all_receiver_fields
             &&
             match snapshot.elem.access with
             | Domain.Access.Read {exp} | Write {exp} ->
                 Option.equal Int.equal
                   (FormalMap.get_formal_index (AccessExpression.get_base exp) helper_formals)
                   (Some 0)
             | ContainerRead _ | ContainerWrite _ | InterfaceCall _ ->
                 false )
           summary.accesses true
    in
    let has_guarded_access =
      receiver_field_accesses
      && Domain.AccessDomain.exists
           (fun (snapshot : Domain.AccessSnapshot.t) ->
             Domain.LockDomain.is_locked snapshot.elem.lock
             && Domain.ThreadsDomain.is_any snapshot.elem.thread )
           summary.accesses
    in
    if
      (not is_private_method)
      || (not attrs.ProcAttributes.is_defined)
      || (not (Int.equal (FormalMap.cardinal helper_formals) 1))
      || (not receiver_field_accesses) || (not has_guarded_access) || summary.never_returns
      || Domain.LockDomain.is_locked summary.locks
      || Domain.LockDomain.has_released summary.locks
    then false
    else
      Procdesc.load pname
      |> Option.exists ~f:(fun helper_desc ->
          let valid = ref true in
          let calls = ref [] in
          let bindings = ref Var.Map.empty in
          let resolve var = Var.Map.find_opt var !bindings in
          (* Call actuals use the production non-dereferencing HIL conversion. *)
          let to_hil (exp, typ) =
            HilExp.of_sil ~include_array_indexes:false ~f_resolve_id:resolve ~add_deref:false exp
              typ
          in
          (* Keep SIL constructor disambiguation principal. *)
          let inspect (instr : Sil.instr) =
            match instr with
            | Sil.Load {id; e; typ} ->
                let access =
                  HilExp.access_expr_of_exp ~include_array_indexes:false ~f_resolve_id:resolve e typ
                in
                bindings := Var.Map.remove (Var.of_id id) !bindings ;
                Option.iter access ~f:(fun access ->
                    bindings := Var.Map.add (Var.of_id id) access !bindings )
            | Store _ | Metadata _ ->
                ()
            | Prune _ ->
                valid := false
            | Call (_, Exp.Const (Const.Cfun callee), actuals, _, _) -> (
                let actuals = List.map actuals ~f:to_hil in
                let role =
                  match ConcurrencyModels.get_lock_effect callee actuals with
                  | GuardConstruct {guard; locks; acquire_now= true}
                    when (not (List.is_empty locks))
                         && List.for_all locks ~f:(function
                           | HilExp.AccessExpression lock -> (
                               FormalMap.is_formal (AccessExpression.get_base lock) helper_formals
                               &&
                               match guard with
                               | HilExp.AccessExpression guard ->
                                   not
                                     (Var.equal
                                        (fst (AccessExpression.get_base lock))
                                        (fst (AccessExpression.get_base guard)) )
                               | _ ->
                                   false )
                           | _ ->
                               false ) ->
                      cpp_guard_role callee
                  | GuardDestroy _ ->
                      cpp_guard_role callee
                  | NoEffect -> (
                    match cpp_guard_role callee with
                    | None ->
                        None
                    | Some _ -> (
                      match analyze_dependency callee with
                      | Ok callee_summary ->
                          let constructor_actuals =
                            match cpp_guard_role callee with
                            | Some (`Acquire, _) ->
                                Some actuals
                            | _ ->
                                None
                          in
                          if
                            paired_guard_summary ?actuals:constructor_actuals analyze_dependency
                              tenv callee callee_summary
                          then cpp_guard_role callee
                          else None
                      | _ ->
                          None ) )
                  | _ ->
                      None
                in
                match (role, List.hd actuals) with
                | Some (role, class_name), Some guard
                  when local_guard_object helper_desc helper_formals guard ->
                    calls := (role, class_name, guard) :: !calls
                | _ ->
                    valid := false )
            | Call _ ->
                valid := false
          in
          let rec walk visited node =
            if not !valid then ()
            else if List.exists visited ~f:(Procdesc.Node.equal node) then valid := false
            else (
              Instrs.iter (Procdesc.Node.get_instrs node) ~f:inspect ;
              match Procdesc.Node.get_succs node with
              | [] ->
                  ()
              | [next] ->
                  walk (node :: visited) next
              | _ ->
                  valid := false )
          in
          walk [] (Procdesc.get_start_node helper_desc) ;
          !valid
          &&
          match List.rev !calls with
          | [(`Acquire, constructor_class, guard); (`Release, destructor_class, destroyed)] ->
              Typ.Name.equal constructor_class destructor_class && HilExp.equal guard destroyed
          | _ ->
              false )


  let process_call_summary analyze_dependency proc_desc tenv formals ret_access_exp callee_pname
      actuals loc astate =
    match analyze_dependency callee_pname with
    | Ok summary ->
        let callee_proc_attrs = Attributes.load_exn callee_pname in
        let callee_guard_evidence =
          ( match cpp_guard_role callee_pname with
            | Some (`Acquire, _) ->
                paired_guard_summary ~actuals analyze_dependency tenv callee_pname summary
            | _ ->
                false )
          && Option.exists (List.hd actuals) ~f:(local_guard_object proc_desc formals)
        in
        let callee_critical_helper =
          direct_receiver_call proc_desc formals callee_pname actuals
          && critical_guard_helper analyze_dependency tenv callee_pname summary callee_proc_attrs
        in
        Domain.integrate_summary formals ~callee_proc_attrs ~callee_guard_evidence
          ~callee_critical_helper summary ret_access_exp callee_pname actuals loc astate
    | Error _ ->
        (* No summary, so nothing is known about this callee's effect on locks. [TouchesLocks] is
           evidence of disturbance, not a proof of its absence, and the chain credit reads it as a
           proof -- so an unresolved call has to count as "may touch". This is where every erased
           call lands: a [std::function], a function pointer, and a virtual call on an abstract
           base all reach here with no body to analyse. Marking costs credit, never soundness. *)
        process_call_without_summary tenv ret_access_exp callee_pname actuals astate
        |> Domain.mark_touches_locks


  (* A modelled call is not analysed, so its lock effect has to be recovered some other way. A
     container model stands for the container's own bookkeeping, not for the ELEMENT code it runs:
     assigning a vector invokes the element's [operator=], resizing one invokes its constructor,
     and a lookup in an ordered container invokes the key's [operator<]. Any of those may be
     user-defined and may touch a lock.

     Take the lock effect from the callee's summary and nothing else -- the model still supplies the
     accesses, which is the whole point of having it. A missing summary is ignorance, not innocence,
     so it counts as "may touch", exactly as in [process_call_summary].

     Consulting the summary is only more precise than marking unconditionally for a container whose
     body is captured, which in practice means a user-defined one. The standard containers are
     declared but not defined, so they have no summary and always take the "may touch" branch: a
     helper whose only container work is on a [std::vector] or a [std::string] keeps no credit. *)
  let join_modelled_callee_lock_effect analyze_dependency callee_pname astate =
    match analyze_dependency callee_pname with
    | Ok (summary : Domain.summary) when not summary.touches_locks ->
        astate
    | _ ->
        Domain.mark_touches_locks astate


  (* Whether a guard owns a lock is not tracked (e.g. a [std::unique_lock] constructed with
     [std::defer_lock] or already unlocked owns none), so a guard constructed by this procedure, in a
     local variable, is assumed to release only a lock acquired by this procedure. A guard reached
     from a formal, a global or a captured variable, such as a member destroyed by the destructor of
     its owner, may release a lock held on entry. *)
  let is_local_guard proc_desc formals guard =
    match HilExp.get_access_exprs guard with
    | [access_exp] ->
        let ((var, _) as base) = AccessExpression.get_base access_exp in
        not
          ( FormalMap.is_formal base formals || Var.is_global var
          || Option.exists (Var.get_pvar var) ~f:(Procdesc.is_captured_pvar proc_desc) )
    | _ ->
        true


  let process_lock_effect_or_summary analyze_dependency proc_desc tenv formals ret_access_exp
      callee_pname actuals loc (astate : Domain.t) =
    let lock_effect = ConcurrencyModels.get_lock_effect callee_pname actuals in
    (* any lock effect at all, acquire or release, disqualifies this procedure from passing its
       callers' lock credit down a chain: see [TouchesLocks] *)
    let astate =
      match lock_effect with NoEffect -> astate | _ -> Domain.mark_touches_locks astate
    in
    match lock_effect with
    | Lock locks ->
        (* [std::lock] acquires all its arguments, which are then released one by one, and a lock
           function from [--lock-model] may take no argument *)
        Fn.apply_n_times ~n:(Int.max 1 (List.length locks)) Domain.acquire_lock astate
    | GuardLock _ ->
        Domain.acquire_lock astate
    | GuardTransfer _ ->
        (* a move used to reach the case below with the source guard as its one lock;
           keep counting it the same way, so that the two destructors stay balanced *)
        Domain.acquire_lock astate
    | GuardConstruct {locks; acquire_now= true} ->
        Fn.apply_n_times ~n:(List.length locks) Domain.acquire_lock astate
    | Unlock _ ->
        Domain.release_lock ~only_acquired:false astate
    | GuardUnlock guard ->
        Domain.release_lock ~only_acquired:(is_local_guard proc_desc formals guard) astate
    | GuardDestroy guard ->
        (* a guard that is not a local variable is assumed to own a lock when destroyed, unless this
           procedure has already released a lock held on entry, e.g. by unlocking the guard *)
        let only_acquired =
          is_local_guard proc_desc formals guard || Domain.LockDomain.has_released astate.locks
        in
        Fn.apply_n_times
          ~n:(RacerDModels.get_guard_destructor_lock_count callee_pname)
          (Domain.release_lock ~only_acquired)
          astate
    | LockedIfTrue _ ->
        Domain.lock_if_true ~guard:false ret_access_exp astate
    | GuardLockedIfTrue _ ->
        Domain.lock_if_true ~guard:true ret_access_exp astate
    | LockedIfZero _ ->
        Domain.lock_if_zero ret_access_exp astate
    | GuardConstruct {acquire_now= false} ->
        astate
    | NoEffect when RacerDModels.proc_is_ignored_by_racerd callee_pname ->
        astate
    | NoEffect ->
        process_call_summary analyze_dependency proc_desc tenv formals ret_access_exp callee_pname
          actuals loc astate


  let process_for_functional_values tenv ret_access_exp callee_pname (astate : Domain.t) =
    let open Domain in
    if PatternMatch.override_exists RacerDModels.is_functional tenv callee_pname then
      let attribute_map = AttributeMapDomain.add ret_access_exp Functional astate.attribute_map in
      {astate with attribute_map}
    else astate


  let process_for_onwership_acquisition tenv ret_access_exp callee_pname astate =
    if
      PatternMatch.override_exists
        (RacerDModels.has_return_annot Annotations.ia_is_returns_ownership)
        tenv callee_pname
    then do_call_acquiring_ownership ret_access_exp astate
    else astate


  let process_for_noreturn callee_pname (astate : Domain.t) =
    if Attributes.is_no_return callee_pname then Domain.branch_never_returns () else astate


  let do_proc_call ret_base callee_pname actuals call_flags loc
      {interproc= {proc_desc; tenv; analyze_dependency}; formals} (astate : Domain.t) =
    let ret_access_exp = AccessExpression.base ret_base in
    process_for_unannotated_interface_call tenv formals call_flags callee_pname actuals loc astate
    |> process_for_thread_assert_effect ret_access_exp callee_pname
    |> process_lock_effect_or_summary analyze_dependency proc_desc tenv formals ret_access_exp
         callee_pname actuals loc
    |> process_for_functional_values tenv ret_access_exp callee_pname
    |> process_for_onwership_acquisition tenv ret_access_exp callee_pname
    |> process_for_noreturn callee_pname


  let do_assignment lhs_access_exp rhs_exp loc {interproc= {tenv}; formals} (astate : Domain.t) =
    let open Domain in
    let astate = add_access tenv formals loc ~is_write:false astate rhs_exp in
    let rhs_access_exprs = HilExp.get_access_exprs rhs_exp in
    let is_functional =
      (not (List.is_empty rhs_access_exprs))
      && List.for_all rhs_access_exprs ~f:(AttributeMapDomain.is_functional astate.attribute_map)
      &&
      match AccessExpression.get_typ lhs_access_exp tenv with
      | Some {Typ.desc= Typ.Tint ILong | Tfloat FDouble} ->
          (* writes to longs and doubles are not guaranteed to be atomic in Java
             (http://docs.oracle.com/javase/specs/jls/se7/html/jls-17.html#jls-17.7), so there
             can be a race even if the RHS is functional *)
          false
      | _ ->
          true
    in
    let astate =
      if is_functional then
        (* we want to forget about writes to @Functional fields altogether, otherwise we'll
           report spurious read/write races *)
        astate
      else add_access tenv formals loc ~is_write:true astate (HilExp.AccessExpression lhs_access_exp)
    in
    let ownership = OwnershipDomain.propagate_assignment lhs_access_exp rhs_exp astate.ownership in
    let attribute_map =
      AttributeMapDomain.propagate_assignment lhs_access_exp rhs_exp astate.attribute_map
    in
    let return_alias =
      ReturnAliasDomain.assign formals ~lhs:lhs_access_exp ~rhs:rhs_exp astate.return_alias
    in
    {astate with ownership; attribute_map; return_alias}


  let do_assume formals assume_exp loc tenv (astate : Domain.t) =
    let open Domain in
    let rec apply_choice bool_value (acc : Domain.t) = function
      | Attribute.(LockHeld | GuardLockHeld) ->
          let locks =
            if bool_value then LockDomain.acquire_lock acc.locks
            else LockDomain.release_acquired_lock acc.locks
          in
          {acc with locks}
      | Attribute.LockHeldIfZero ->
          apply_choice (not bool_value) acc Attribute.LockHeld
      | Attribute.OnMainThread ->
          let threads =
            if bool_value then ThreadsDomain.AnyThreadButSelf else ThreadsDomain.AnyThread
          in
          {acc with threads}
      | Attribute.(Functional | Nothing | Synchronized) ->
          acc
    in
    (* trylocks returning zero on success return negative error codes, so [r < 0] means [r != 0] *)
    let rec zero_status_as_equality (exp : HilExp.t) : HilExp.t =
      match exp with
      | BinaryOperator (Lt, e1, e2) when HilExp.is_int_zero e2 ->
          BinaryOperator (Ne, e1, e2)
      | BinaryOperator (Ge, e1, e2) when HilExp.is_int_zero e2 ->
          BinaryOperator (Eq, e1, e2)
      | BinaryOperator (Gt, e1, e2) when HilExp.is_int_zero e1 ->
          BinaryOperator (Ne, e1, e2)
      | BinaryOperator (Le, e1, e2) when HilExp.is_int_zero e1 ->
          BinaryOperator (Eq, e1, e2)
      | UnaryOperator (LNot, e, typ) ->
          UnaryOperator (LNot, zero_status_as_equality e, typ)
      | Cast (typ, e) ->
          Cast (typ, zero_status_as_equality e)
      | _ ->
          exp
    in
    let astate = add_access tenv formals loc ~is_write:false astate assume_exp in
    match HilExp.get_access_exprs assume_exp with
    | [access_expr] ->
        let attribute = AttributeMapDomain.get access_expr astate.attribute_map in
        let assume_exp =
          match attribute with
          | Attribute.LockHeldIfZero ->
              zero_status_as_equality assume_exp
          | _ ->
              assume_exp
        in
        HilExp.eval_boolean_exp access_expr assume_exp
        |> Option.value_map ~default:astate ~f:(fun bool_value ->
            (* prune (prune_exp) can only evaluate to true if the choice is [bool_value].
                  add the constraint that the choice must be [bool_value] to the state *)
            apply_choice bool_value astate attribute )
    | _ ->
        astate


  let exec_instr astate
      ({interproc= {proc_desc; tenv; analyze_dependency}; formals} as analysis_data) _ _ instr =
    match (instr : HilInstr.t) with
    | Call (ret_base, Direct callee_pname, actuals, call_flags, loc) ->
        let astate = Domain.add_reads_of_hilexps tenv formals actuals loc astate in
        (* The container branches model the call instead of analysing it, so neither reaches
           [process_call_summary]; they recover the callee's lock effect from its summary instead,
           because the element code a container runs is not visible here. The ownership branch
           does not: in C++ it matches only [__new] and [__new_array], which allocate raw memory
           and run no user code -- the constructor is a separate call that is analysed normally.
           (In Java it also matches [clone] and [newInstance], which can run user code, but the
           chain credit this feeds is C++-only.) *)
        if RacerDModels.acquires_ownership callee_pname tenv then
          do_call_acquiring_ownership (AccessExpression.base ret_base) astate
        else if RacerDModels.is_container_write tenv callee_pname then
          Domain.add_container_access tenv formals ~is_write:true ret_base callee_pname actuals loc
            astate
          |> join_modelled_callee_lock_effect analyze_dependency callee_pname
        else if RacerDModels.is_container_read tenv callee_pname then
          Domain.add_container_access tenv formals ~is_write:false ret_base callee_pname actuals loc
            astate
          |> join_modelled_callee_lock_effect analyze_dependency callee_pname
        else do_proc_call ret_base callee_pname actuals call_flags loc analysis_data astate
    | Call (_, Indirect _, _, _, _) ->
        if Procname.is_java (Procdesc.get_proc_name proc_desc) then
          L.(die InternalError) "Unexpected indirect call instruction %a" HilInstr.pp instr
        else
          (* a call through a value this analysis cannot name: nothing is known about it, least of
             all whether it touches a lock *)
          Domain.mark_touches_locks astate
    | Assign (lhs_access_expr, rhs_exp, loc) ->
        do_assignment lhs_access_expr rhs_exp loc analysis_data astate
    | Assume (assume_exp, _, _, loc) ->
        do_assume formals assume_exp loc tenv astate
    | Metadata _ ->
        astate


  let call_return_alias {interproc= {tenv; analyze_dependency}} callee_pname actuals =
    (* the calls for which [exec_instr] applies the callee summary *)
    let is_summarized_call () =
      (not (RacerDModels.acquires_ownership callee_pname tenv))
      && (not (RacerDModels.is_container_write tenv callee_pname))
      && (not (RacerDModels.is_container_read tenv callee_pname))
      &&
      match ConcurrencyModels.get_lock_effect callee_pname actuals with
      | NoEffect ->
          not (RacerDModels.proc_is_ignored_by_racerd callee_pname)
      | _ ->
          false
    in
    if ReturnAliasDomain.is_supported_procname callee_pname && is_summarized_call () then
      match analyze_dependency callee_pname with
      (* an attribute of the returned value, e.g. [LockHeld], is attached to the return identifier
         and would be lost by resolving it to the alias *)
      | Ok {Domain.return_alias= Some alias; return_attribute= Nothing} ->
          ReturnAliasDomain.subst ~callee:callee_pname actuals alias
      | _ ->
          None
    else None


  let pp_session_name _node fmt = F.pp_print_string fmt "racerd"
end

module Analyzer = LowerHil.MakeAbstractInterpreter (TransferFunctions (ProcCfg.Normal))

let analyze ({InterproceduralAnalysis.proc_desc; tenv} as interproc) =
  let open RacerDDomain in
  let proc_attrs = Procdesc.get_attributes proc_desc in
  let proc_name = proc_attrs.proc_name in
  let open ConcurrencyModels in
  let add_owned_formal acc base = OwnershipDomain.add base OwnershipAbstractValue.owned acc in
  let add_conditionally_owned_formal =
    let is_owned_formal {Annot.class_name} =
      (* [@InjectProp] allocates a fresh object to bind to the parameter *)
      String.is_suffix ~suffix:Annotations.inject_prop class_name
    in
    let is_inject_prop =
      Annotations.method_has_annotation_with proc_attrs.ret_annots
        (List.map (Procdesc.get_formals proc_desc) ~f:trd3)
        is_owned_formal
    in
    fun acc formal formal_index ->
      let ownership_value =
        if is_inject_prop then OwnershipAbstractValue.owned
        else OwnershipAbstractValue.make_owned_if formal_index
      in
      OwnershipDomain.add formal ownership_value acc
  in
  if RacerDModels.should_analyze_proc tenv proc_name then
    let locks =
      if Procdesc.is_java_synchronized proc_desc || Procdesc.is_csharp_synchronized proc_desc then
        LockDomain.(acquire_lock initial)
      else LockDomain.initial
    in
    let threads =
      if
        runs_on_ui_thread tenv proc_name
        || RacerDModels.is_thread_confined_method tenv proc_name
        || RacerDModels.is_confined_by_contract tenv proc_name
      then ThreadsDomain.AnyThreadButSelf
      else if
        Procdesc.is_java_synchronized proc_desc
        || Procdesc.is_csharp_synchronized proc_desc
        || RacerDModels.is_marked_thread_safe proc_name tenv
      then ThreadsDomain.AnyThread
      else ThreadsDomain.NoThread
    in
    let ownership =
      let is_initializer = RacerDModels.is_initializer tenv proc_name in
      let is_injected =
        is_initializer && Annotations.pdesc_has_return_annot proc_desc Annotations.ia_is_inject
      in
      Procdesc.get_formals proc_desc
      |> List.foldi ~init:OwnershipDomain.empty ~f:(fun index acc (name, typ, _) ->
          let base =
            AccessPath.base_of_pvar (Pvar.mk name proc_name) typ |> AccessExpression.base
          in
          if is_injected then
            (* if a constructor is called via DI, all of its formals will be freshly allocated and
                  therefore owned. we assume that constructors annotated with [@Inject] will only be
                  called via DI or using fresh parameters. *)
            add_owned_formal acc base
          else if is_initializer && Int.equal 0 index then
            (* express that the constructor owns [this] *)
            add_owned_formal acc base
          else add_conditionally_owned_formal acc base index )
    in
    let uses_locks =
      Procdesc.is_java_synchronized proc_desc || Procdesc.is_csharp_synchronized proc_desc
    in
    let initial = {initial with ownership; threads; locks; uses_locks} in
    let formals = FormalMap.make proc_attrs in
    let analysis_data = {interproc; formals} in
    Analyzer.compute_post analysis_data ~initial proc_desc
    |> Option.map ~f:(astate_to_summary proc_desc formals)
  else Some empty_summary
