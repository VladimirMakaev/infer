(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

open! IStd
module F = Format
module L = Logging

(** backward analysis for computing set of maybe-live variables at each program point *)

module VarSet = AbstractDomain.FiniteSet (Var)
module Domain = VarSet

module LocalAliases = struct
  module Target = struct
    type t = Local of Pvar.t | Unknown [@@deriving compare]

    let pp f = function
      | Local pvar ->
          Pvar.pp_value f pvar
      | Unknown ->
          F.pp_print_string f "unknown"
  end

  module Targets = AbstractDomain.FiniteSet (Target)
  module Domain = AbstractDomain.Map (Var) (Targets)

  let unknown = Targets.singleton Target.Unknown

  let get aliases var = Option.value (Domain.find_opt var aliases) ~default:unknown

  let rec address aliases exp =
    match Exp.ignore_cast exp with
    | Exp.Lvar pvar ->
        Targets.singleton (Target.Local pvar)
    | Exp.Var id ->
        get aliases (Var.of_id id)
    | Exp.Lindex (exp, Const (Cint offset))
    | Exp.BinOp ((PlusPI | MinusPI), exp, Const (Cint offset))
      when IntLit.iszero offset ->
        address aliases exp
    | Exp.Lfield ({exp}, _, _) | Exp.Lindex (exp, _) | Exp.BinOp ((PlusPI | MinusPI), exp, _) ->
        (* An offset may designate only part of a variable, so it cannot justify a strong kill. *)
        Targets.add Target.Unknown (address aliases exp)
    | _ ->
        unknown


  let load aliases exp =
    Targets.fold
      (fun target acc ->
        match target with
        | Target.Local pvar ->
            Targets.union acc (get aliases (Var.of_pvar pvar))
        | Target.Unknown ->
            Targets.add Target.Unknown acc )
      (address aliases exp) Targets.empty


  let locals targets =
    Targets.fold
      (fun target acc ->
        match target with
        | Target.Local pvar ->
            VarSet.add (Var.of_pvar pvar) acc
        | Target.Unknown ->
            acc )
      targets VarSet.empty


  let reachable aliases targets =
    let rec visit todo seen =
      match VarSet.choose_opt todo with
      | None ->
          seen
      | Some var ->
          let todo = VarSet.remove var todo in
          if VarSet.mem var seen then visit todo seen
          else visit (VarSet.union todo (locals (get aliases var))) (VarSet.add var seen)
    in
    visit (locals targets) VarSet.empty


  let reachable_exp aliases exp = reachable aliases (address aliases exp)

  let singleton_local targets =
    if Int.equal (Targets.cardinal targets) 1 then
      match Targets.choose_opt targets with Some (Target.Local pvar) -> Some pvar | _ -> None
    else None


  let store_escapes aliases escaped lhs =
    let rec root exp =
      match Exp.ignore_cast exp with
      | Exp.Lfield ({exp}, _, _) | Exp.Lindex (exp, _) | Exp.BinOp ((PlusPI | MinusPI), exp, _) ->
          root exp
      | exp ->
          exp
    in
    let lhs = root lhs in
    let targets = address aliases lhs in
    (match lhs with Exp.Lvar pvar -> Pvar.is_global pvar || Pvar.is_return pvar | _ -> false)
    || Targets.mem Target.Unknown targets
    || VarSet.exists (fun var -> VarSet.mem var escaped) (locals targets)


  let initial proc_desc =
    let pointer_names =
      List.filter_map (Procdesc.get_locals proc_desc) ~f:(fun local ->
          Option.some_if (Typ.is_pointer local.ProcAttributes.typ) local.ProcAttributes.name )
      @ List.filter_map (Procdesc.get_formals proc_desc) ~f:(fun (name, typ, _) ->
          Option.some_if (Typ.is_pointer typ) name )
      |> Mangled.Set.of_list
    in
    let add_var aliases var =
      match Var.get_pvar var with
      | None ->
          Domain.add var unknown aliases
      | Some pvar
        when (not (Pvar.is_global pvar)) && Mangled.Set.mem (Pvar.get_name pvar) pointer_names ->
          Domain.add var unknown aliases
      | Some _ ->
          (* Aggregate fields are not pointer cells: do not merge their contents into the object. *)
          aliases
    in
    (* Seed every key with unknown, so a branch that leaves a pointer untouched is not lost at join. *)
    Procdesc.fold_instrs proc_desc ~init:Domain.empty ~f:(fun aliases _ instr ->
        let expressions, aliases =
          match instr with
          | Sil.Load {id; typ= {desc= Tptr _}} ->
              (Sil.exps_of_instr instr, Domain.add (Var.of_id id) unknown aliases)
          | Store {typ= {desc= Tptr _}} ->
              (Sil.exps_of_instr instr, aliases)
          | Call ((id, typ), _, actuals, _, _) ->
              let aliases =
                match typ.Typ.desc with
                | Tptr _ ->
                    Domain.add (Var.of_id id) unknown aliases
                | _ ->
                    aliases
              in
              ( List.filter_map actuals ~f:(fun (exp, typ) ->
                    match typ.Typ.desc with Tptr _ -> Some exp | _ -> None )
              , aliases )
          | _ ->
              ([], aliases)
        in
        List.fold expressions ~init:aliases ~f:(fun aliases exp ->
            Var.get_all_vars_in_exp exp |> Sequence.fold ~init:aliases ~f:add_var ) )


  let store aliases lhs rhs typ =
    let value = match typ.Typ.desc with Tptr _ -> address aliases rhs | _ -> unknown in
    let targets = address aliases lhs in
    match singleton_local targets with
    | Some pvar when Domain.mem (Var.of_pvar pvar) aliases ->
        Domain.add (Var.of_pvar pvar) value aliases
    | Some _ | None ->
        VarSet.fold
          (fun var aliases ->
            if Domain.mem var aliases then
              Domain.add var (Targets.union (get aliases var) value) aliases
            else aliases )
          (locals targets) aliases
end

module Exn = struct
  module CExn = AbstractDomain.Map (Int) (VarSet)

  (* We pair a C-liveset (for C++ exceptions) and a Java-liveset (for Java exceptions) *)
  include AbstractDomain.PairWithBottom (CExn) (VarSet)
end

module ExtendedDomain = struct
  type t = {normal: VarSet.t; exn: Exn.t}

  (* We only pretty-print the normal component of the abstract state *)
  let pp f {normal; exn= _} = F.fprintf f "@[normal:%a@]" VarSet.pp normal

  let leq ~lhs ~rhs =
    VarSet.leq ~lhs:lhs.normal ~rhs:rhs.normal && Exn.leq ~lhs:lhs.exn ~rhs:rhs.exn


  let join x y = {normal= VarSet.join x.normal y.normal; exn= Exn.join x.exn y.exn}

  let widen ~prev ~next ~num_iters =
    { normal= VarSet.widen ~prev:prev.normal ~next:next.normal ~num_iters
    ; exn= Exn.widen ~prev:prev.exn ~next:next.exn ~num_iters }


  let filter_normal {normal} = {normal; exn= Exn.bottom}

  let filter_exceptional {exn= _, j_exn} = {normal= VarSet.bottom; exn= (Exn.CExn.bottom, j_exn)}

  let normal_to_exceptional {normal; exn} =
    {normal= VarSet.bottom; exn= Exn.join exn (Exn.CExn.bottom, normal)}


  let exceptional_to_normal {exn= c_exn, j_exn} =
    {normal= Exn.CExn.fold (fun _ -> VarSet.join) c_exn j_exn; exn= Exn.bottom}


  let bottom = {normal= VarSet.bottom; exn= Exn.bottom}

  let is_bottom {normal; exn} = VarSet.is_bottom normal && Exn.is_bottom exn

  let update_normal ~f x = {x with normal= f x.normal}

  let add var = update_normal ~f:(VarSet.add var)

  let remove var = update_normal ~f:(VarSet.remove var)

  let map_normal ~f x = f x.normal

  let mem var = map_normal ~f:(VarSet.mem var)

  let catch_entry try_id {normal; exn= c_exn, j_exn} =
    {normal= VarSet.empty; exn= (Exn.CExn.add try_id normal c_exn, j_exn)}


  let try_entry try_id {normal; exn= c_exn, j_exn} =
    {normal; exn= (Exn.CExn.remove try_id c_exn, j_exn)}


  let add_live_in_catch {normal; exn= (c_exn, _) as exn} =
    { normal= Exn.CExn.fold (fun _ live_in_catch acc -> VarSet.join acc live_in_catch) c_exn normal
    ; exn }
end

(** only kill pvars that are local; don't kill those that can escape *)
let is_always_in_scope proc_desc pvar =
  Pvar.is_return pvar || Pvar.is_global pvar || Procdesc.is_captured_pvar proc_desc pvar


let json_error ~option_name ~expected ~actual =
  L.die UserError "When parsing option %s: expected %s but got '%s'" option_name expected
    (Yojson.Safe.Util.to_string actual)


let string_list_of_json ~option_name ~init = function
  | `List json ->
      List.fold json
        ~f:(fun acc json ->
          match json with
          | `String s ->
              s :: acc
          | _ ->
              json_error ~option_name ~expected:"string" ~actual:json )
        ~init
  | json ->
      json_error ~option_name ~expected:"list of strings" ~actual:json


module type LivenessConfig = sig
  val is_dangerous_destructor : Procname.t -> bool
end

(** Use this config to get a reliable liveness pre-analysis that tells you which variables are live
    at which program point *)
module PreAnalysisMode : LivenessConfig = struct
  (** do not do any funky stuff *)
  let is_dangerous_destructor _proc_name = false
end

(** Use this config to get a dead store checker that can take some liberties wrt a strict liveness
    analysis *)
module CheckerMode = struct
  let dangerous_destructor_matcher =
    QualifiedCppName.Match.of_fuzzy_qual_names
      (string_list_of_json ~option_name:"liveness-dangerous-classes" ~init:[]
         Config.liveness_dangerous_classes )


  (** hardcoded list of wrappers, mostly because they are impossible to specify as config options *)
  let standard_wrappers_matcher =
    QualifiedCppName.Match.of_fuzzy_qual_names ["std::unique_ptr"; "std::shared_ptr"]


  let is_dangerous_class_name class_name =
    Typ.Name.unqualified_name class_name
    |> QualifiedCppName.Match.match_qualifiers dangerous_destructor_matcher


  let is_wrapper_of_dangerous_class_name class_name =
    Typ.Name.unqualified_name class_name
    |> QualifiedCppName.Match.match_qualifiers standard_wrappers_matcher
    &&
    match Typ.Name.get_template_spec_info class_name with
    | Some (Template {args= TType {desc= Tstruct name} :: _; _}) ->
        is_dangerous_class_name name
    | _ ->
        false


  let is_dangerous_proc_name (proc_name : Procname.t) =
    match proc_name with
    | ObjC_Cpp cpp_pname ->
        is_dangerous_class_name cpp_pname.class_name
        || is_wrapper_of_dangerous_class_name cpp_pname.class_name
    | _ ->
        false


  let is_destructor (proc_name : Procname.t) =
    match proc_name with
    | ObjC_Cpp cpp_pname ->
        Procname.ObjC_Cpp.is_destructor cpp_pname
    | _ ->
        false


  let is_dangerous_destructor (proc_name : Procname.t) =
    is_destructor proc_name && is_dangerous_proc_name proc_name
end

(** compilers 101-style backward transfer functions for liveness analysis. gen a variable when it is
    read, kill the variable when it is assigned *)
module TransferFunctions (LConfig : LivenessConfig) (CFG : ProcCfg.S) = struct
  module CFG = CFG
  module Domain = ExtendedDomain

  type analysis_data = Procdesc.t

  (** add all of the vars read in [exp] to the live set *)
  let exp_add_live exp astate =
    let astate' =
      Exp.free_vars exp
      |> Sequence.fold ~init:astate ~f:(fun astate_acc id -> Domain.add (Var.of_id id) astate_acc)
    in
    Exp.program_vars exp
    |> Sequence.fold ~init:astate' ~f:(fun astate_acc pvar ->
        Domain.add (Var.of_pvar pvar) astate_acc )


  let add_live_actuals actuals astate_acc =
    let actuals = List.map actuals ~f:(fun (e, _) -> Exp.ignore_cast e) in
    List.fold actuals ~f:(fun acc_ exp -> exp_add_live exp acc_) ~init:astate_acc


  let exec_instr_normal astate proc_desc = function
    | Sil.Load {id= lhs_id} when Ident.is_none lhs_id ->
        (* dummy deref inserted by frontend--don't count as a read *)
        astate
    | Sil.Load {id= lhs_id; e= rhs_exp} ->
        Domain.remove (Var.of_id lhs_id) astate |> exp_add_live rhs_exp
    | Sil.Store {e1= Lvar lhs_pvar; e2= rhs_exp} ->
        let astate' =
          if is_always_in_scope proc_desc lhs_pvar then astate (* never kill globals *)
          else Domain.remove (Var.of_pvar lhs_pvar) astate
        in
        exp_add_live rhs_exp astate'
    | Sil.Store {e1= lhs_exp; e2= rhs_exp} ->
        exp_add_live lhs_exp astate |> exp_add_live rhs_exp
    | Sil.Prune (exp, _, _, _) ->
        exp_add_live exp astate
    | Sil.Call ((ret_id, _), Const (Cfun callee_pname), _, _, _)
      when LConfig.is_dangerous_destructor callee_pname ->
        Logging.d_printfln_escaped "Dangerous destructor %a, ignoring reads@\n" Procname.pp
          callee_pname ;
        Domain.remove (Var.of_id ret_id) astate
    | Sil.Call ((ret_id, _), call_exp, actuals, _, {CallFlags.cf_assign_last_arg}) ->
        let actuals_to_read, astate =
          if cf_assign_last_arg then
            match IList.split_last_rev actuals with
            | Some ((Exp.Lvar pvar, _), actuals') when not (is_always_in_scope proc_desc pvar) ->
                (actuals', Domain.remove (Var.of_pvar pvar) astate)
            | _ ->
                (actuals, astate)
          else (actuals, astate)
        in
        Domain.remove (Var.of_id ret_id) astate
        |> exp_add_live call_exp |> add_live_actuals actuals_to_read
        |>
        (* assume that all function calls can throw for now *)
        Domain.add_live_in_catch
    | Sil.Metadata (CatchEntry {try_id}) ->
        Domain.catch_entry try_id astate
    | Sil.Metadata (TryEntry {try_id}) ->
        Domain.try_entry try_id astate
    | Sil.Metadata _ ->
        astate


  let exec_instr astate proc_desc _ _ instr =
    (* A variable is live before [instr] if it is:
       - live after instr and not set by [instr]
       - or it used by [instr]
       - or it is live before an exceptional successor node *)
    let astate_normal = exec_instr_normal astate proc_desc instr in
    Domain.(filter_exceptional astate |> exceptional_to_normal |> join astate_normal)


  let join_all astates ~into =
    List.fold astates ~init:into ~f:(fun acc astate ->
        Some (Option.value_map acc ~default:astate ~f:(fun acc -> Domain.join acc astate)) )


  let filter_normal = Domain.filter_normal

  let filter_exceptional = Domain.filter_exceptional

  let transform_on_exceptional_edge = Domain.normal_to_exceptional

  let pp_session_name node fmt = F.fprintf fmt "liveness %a" CFG.Node.pp_id (CFG.Node.id node)
end

module CFG = ProcCfg.OneInstrPerNode (ProcCfg.Backward (ProcCfg.Exceptional))
module PreAnalysisTransferFunctions = TransferFunctions (PreAnalysisMode)
module BackwardCfg = ProcCfg.Backward (ProcCfg.Exceptional)
module Iter = AbstractInterpreter.MakeBackwardRPO (PreAnalysisTransferFunctions (BackwardCfg))

type t = ExtendedDomain.t AbstractInterpreter.State.t Iter.InvariantMap.t

let compute proc_desc =
  let liveness_proc_cfg = BackwardCfg.from_pdesc proc_desc in
  let initial = ExtendedDomain.bottom in
  Iter.exec_cfg liveness_proc_cfg ~initial proc_desc


(* note: because the analysis is backward, post and pre are reversed *)
let live_before node_id inv_map =
  let f {AbstractInterpreter.State.post} = post.ExtendedDomain.normal in
  Iter.extract_state node_id inv_map |> Option.map ~f


let live_after node_id inv_map =
  let f {AbstractInterpreter.State.pre} =
    let {ExtendedDomain.normal; exn= _, j_exn} = pre in
    Domain.join normal j_exn
  in
  Iter.extract_state node_id inv_map |> Option.map ~f


(* It's fine to have a dead store on a type that uses the "scope guard" pattern. These types
   are only read in their destructors, and this is expected/ok.
   (e.g., https://github.com/facebook/folly/blob/master/folly/ScopeGuard.h). *)
let matcher_scope_guard =
  let default_scope_guards = ["CKComponentKey"; "CKComponentScope"] in
  string_list_of_json ~option_name:"cxx-scope_guards" ~init:default_scope_guards
    Config.cxx_scope_guards
  |> QualifiedCppName.Match.of_fuzzy_qual_names


module PassedByRefTransferFunctions (CFG : ProcCfg.S) = struct
  module CFG = CFG
  module RefDomain = AbstractDomain.PairWithBottom (LocalAliases.Domain) (VarSet)
  module Domain = AbstractDomain.PairWithBottom (RefDomain) (VarSet)

  type analysis_data = unit

  let add_actual aliases expr escaped =
    VarSet.union escaped (LocalAliases.reachable_exp aliases expr)


  let add_direct expr vars =
    match Exp.ignore_cast expr with
    | Exp.Lvar pvar ->
        VarSet.add (Var.of_pvar pvar) vars
    | _ ->
        vars


  let proc_name_of_expr expr =
    match (expr : Exp.t) with Const (Cfun proc_name) -> Some proc_name | _ -> None


  let is_skip expr =
    proc_name_of_expr expr |> Option.exists ~f:(Procname.equal BuiltinDecl.__infer_skip)


  let is_dangerous expr =
    (* ignore all captures from "dangerous" classes *)
    proc_name_of_expr expr |> Option.exists ~f:CheckerMode.is_dangerous_proc_name


  let exec_instr ((aliases, passed_by_ref), escaped) () _ _ (instr : Sil.instr) =
    let passed_by_ref, escaped =
      match instr with
      | Call (_, f, actuals, _, _) when is_skip f ->
          (* Explicit value-use markers do not read pointees or let addresses escape. Preserve
             their existing direct-variable reporting suppression separately from real escapes. *)
          ( List.fold actuals ~init:passed_by_ref ~f:(fun passed_by_ref (exp, _) ->
                add_direct exp passed_by_ref )
          , escaped )
      | Call (_ret, f, actuals, _loc, _flags) when not (is_dangerous f) ->
          let actuals =
            if Option.exists (proc_name_of_expr f) ~f:Procname.is_constructor then
              (* Skip "this" in constructors, assuming constructors are less likely to have global
                 side effects that store "this" in the global state. We could also skip "this" in
                 all (non-static) methods but this becomes less clear: constructing an object then
                 calling methods on it can have side-effects even if the object is used for nothing
                 else. *)
              List.tl actuals |> Option.value ~default:[]
            else actuals
          in
          let targets =
            List.fold actuals ~init:VarSet.empty ~f:(fun targets (actual, _typ) ->
                add_actual aliases actual targets )
          in
          (VarSet.union passed_by_ref targets, VarSet.union escaped targets)
      | Store {e1; e2; _} when LocalAliases.store_escapes aliases escaped e1 ->
          let targets = LocalAliases.reachable_exp aliases e2 in
          (VarSet.union passed_by_ref targets, VarSet.union escaped targets)
      | _ ->
          (passed_by_ref, escaped)
    in
    let captured =
      List.fold (Sil.exps_of_instr instr) ~init:VarSet.empty ~f:(fun captured exp ->
          (* Copying a pointer into a closure does not read its pointee. Keep the existing direct
             reference-capture handling; closure fields and body summaries are outside these facts. *)
          Exp.fold_captured exp ~f:(fun captured exp -> add_direct exp captured) captured )
    in
    let passed_by_ref = VarSet.union passed_by_ref captured in
    let escaped = VarSet.union escaped captured in
    let aliases =
      match instr with
      | Load {id; e; typ} ->
          if LocalAliases.Domain.mem (Var.of_id id) aliases then
            let value =
              match typ.Typ.desc with
              | Tptr _ ->
                  LocalAliases.load aliases e
              | _ ->
                  LocalAliases.unknown
            in
            LocalAliases.Domain.add (Var.of_id id) value aliases
          else aliases
      | Store {e1; e2; typ} ->
          LocalAliases.store aliases e1 e2 typ
      | Call (_, f, _, _, _) when is_skip f ->
          aliases
      | Call ((id, _), _, _, _, _) ->
          (* A call can change pointer cells whose addresses escaped, including earlier calls. *)
          VarSet.fold
            (fun var aliases ->
              if LocalAliases.Domain.mem var aliases then
                LocalAliases.Domain.add var LocalAliases.unknown aliases
              else aliases )
            escaped aliases
          |> fun aliases ->
          if LocalAliases.Domain.mem (Var.of_id id) aliases then
            LocalAliases.Domain.add (Var.of_id id) LocalAliases.unknown aliases
          else aliases
      | _ ->
          aliases
    in
    ((aliases, passed_by_ref), escaped)


  let pp_session_name _node fmt = F.pp_print_string fmt "passed by reference"
end

module AliasCFG = ProcCfg.OneInstrPerNode (ProcCfg.Exceptional)
module RefTransferFunctions = PassedByRefTransferFunctions (AliasCFG)
module PassedByRefAnalyzer = AbstractInterpreter.MakeRPO (RefTransferFunctions)

let get_passed_by_ref_invariant_map proc_desc =
  let cfg = AliasCFG.from_pdesc proc_desc in
  PassedByRefAnalyzer.exec_cfg cfg ()
    ~initial:((LocalAliases.initial proc_desc, VarSet.empty), VarSet.empty)


let alias_node_id (node, backward_index) =
  let index = max 0 (Instrs.count (Procdesc.Node.get_instrs node) - 1 - backward_index) in
  (Procdesc.Node.get_id node, index)


module CheckerTransferFunctions = struct
  module Base = TransferFunctions (CheckerMode) (CFG)
  include Base

  type analysis_data = Procdesc.t * PassedByRefAnalyzer.invariant_map

  let exec_instr astate (proc_desc, aliases_map) node index (instr : Sil.instr) =
    let aliases, escaped =
      PassedByRefAnalyzer.extract_pre (alias_node_id node) aliases_map
      |> Option.value_map ~default:(LocalAliases.Domain.empty, VarSet.empty)
           ~f:(fun ((aliases, _), escaped) -> (aliases, escaped) )
    in
    let astate =
      match (instr : Sil.instr) with
      | Store {e1= Lvar _} ->
          (* The base transfer already kills direct stores. *)
          astate
      | Store {e1; typ} -> (
        match LocalAliases.singleton_local (LocalAliases.address aliases e1) with
        | Some pvar when not (is_always_in_scope proc_desc pvar) ->
            let name = Pvar.get_name pvar in
            let same_type =
              List.exists (Procdesc.get_locals proc_desc) ~f:(fun local ->
                  Mangled.equal name local.ProcAttributes.name
                  && Typ.equal typ local.ProcAttributes.typ )
              || List.exists (Procdesc.get_formals proc_desc) ~f:(fun (formal, formal_typ, _) ->
                  Mangled.equal name formal && Typ.equal typ formal_typ )
            in
            if same_type then Domain.remove (Var.of_pvar pvar) astate else astate
        | _ ->
            astate )
      | _ ->
          astate
    in
    let astate = Base.exec_instr astate proc_desc node index instr in
    let add_pointees exp astate =
      VarSet.fold Domain.add (LocalAliases.locals (LocalAliases.address aliases exp)) astate
    in
    let add_reachable exp astate =
      VarSet.fold Domain.add (LocalAliases.reachable_exp aliases exp) astate
    in
    match instr with
    | Load {id; e} when not (Ident.is_none id) ->
        add_pointees e astate
    | Call (_, call_exp, actuals, _, {CallFlags.cf_assign_last_arg})
      when not (RefTransferFunctions.is_dangerous call_exp || RefTransferFunctions.is_skip call_exp)
      ->
        let actuals =
          if cf_assign_last_arg then
            match IList.split_last_rev actuals with
            | Some ((Exp.Lvar pvar, _), rest) when not (is_always_in_scope proc_desc pvar) ->
                rest
            | _ ->
                actuals
          else actuals
        in
        List.fold actuals ~init:astate ~f:(fun astate (exp, _) -> add_reachable exp astate)
    | Store {e1; e2; _} when LocalAliases.store_escapes aliases escaped e1 ->
        add_reachable e2 astate
    | _ ->
        astate
end

module CheckerAnalyzer = AbstractInterpreter.MakeRPO (CheckerTransferFunctions)
module IntLitSet = Stdlib.Set.Make (IntLit)

let ignored_constants =
  let int_lit_constants =
    List.map
      ~f:(fun el ->
        try IntLit.of_string el
        with Invalid_argument _ ->
          L.die UserError
            "Ill-formed option  '%s' for --liveness-ignored-constant: an integer was expected" el )
      Config.liveness_ignored_constant
  in
  IntLitSet.of_list int_lit_constants


(** Whether the C++ constructor [pname] only writes to the object it constructs and to its own
    locals, so that building an object that is never read has no effect. Constructors without a body
    qualify only when they are compiler-generated. Constructors calling other constructors are
    followed [depth] levels deep. *)
let rec constructor_writes_only_to_this ~depth pname =
  match Procdesc.load pname with
  | Some pdesc when Procdesc.is_defined pdesc ->
      depth > 0 && body_writes_only_to_this ~depth pdesc
  | _ ->
      Attributes.load pname |> Option.exists ~f:(fun attrs -> attrs.ProcAttributes.is_cpp_implicit)


and body_writes_only_to_this ~depth pdesc =
  let this_ids =
    Procdesc.fold_instrs pdesc ~init:Ident.Set.empty ~f:(fun ids _node instr ->
        match instr with
        | Sil.Load {id; e= Lvar pvar} when Pvar.is_this pvar ->
            Ident.Set.add id ids
        | _ ->
            ids )
  in
  let rec is_owned (addr : Exp.t) =
    match addr with
    | Lfield ({exp}, _, _) | Lindex (exp, _) | Cast (_, exp) ->
        is_owned exp
    | Var id ->
        Ident.Set.mem id this_ids
    | Lvar pvar ->
        not (Pvar.is_global pvar)
    | _ ->
        false
  in
  let is_pure_builtin pname =
    List.mem ~equal:Procname.equal
      [BuiltinDecl.__cast; BuiltinDecl.__infer_skip; BuiltinDecl.__new; BuiltinDecl.__new_array]
      pname
  in
  let instr_writes_only_to_this (instr : Sil.instr) =
    match instr with
    | Store {e1} ->
        is_owned e1
    | Call (_, Const (Cfun callee), _, _, _) when is_pure_builtin callee ->
        true
    | Call (_, Const (Cfun callee), (this_arg, _) :: _, _, _) when Procname.is_constructor callee ->
        is_owned this_arg && constructor_writes_only_to_this ~depth:(depth - 1) callee
    | Call _ ->
        false
    | Load _ | Prune _ | Metadata _ ->
        true
  in
  Procdesc.fold_instrs pdesc ~init:true ~f:(fun ok _node instr ->
      ok && instr_writes_only_to_this instr )


(** Whether a constructor call with arguments [actuals] copies or moves an object of the same type
    passed by reference. Such objects are not kept for the effects of their construction, whatever
    these effects are on the source. *)
let is_copy_or_move_constructor_call actuals =
  match actuals with
  | [ (_, {Typ.desc= Tptr (this_typ, Pk_pointer)})
    ; (_, {Typ.desc= Tptr (src_typ, (Pk_lvalue_reference | Pk_rvalue_reference))}) ] -> (
    match (Typ.name this_typ, Typ.name src_typ) with
    | Some this_name, Some src_name ->
        Typ.Name.equal this_name src_name
    | _ ->
        false )
  | _ ->
      false


(** Names of the program variables that the bodies of the closures created in [proc_desc], or in
    closures nested in them, read without binding them. A C++ lambda reads a constant of the
    enclosing procedure without capturing it when the read is not an odr-use, so that read only
    appears in the lambda's body. *)
let names_read_by_closures proc_desc =
  let fold_exps pdesc ~init ~f =
    Procdesc.fold_instrs pdesc ~init ~f:(fun acc _node instr ->
        List.fold (Sil.exps_of_instr instr) ~init:acc ~f )
  in
  let closures_of pdesc =
    fold_exps pdesc ~init:Procname.Set.empty ~f:(fun acc exp ->
        Exp.closures exp
        |> Sequence.fold ~init:acc ~f:(fun acc {Exp.name} -> Procname.Set.add name acc) )
  in
  let rec free_names ~ancestors closure_name =
    if Procname.Set.mem closure_name ancestors then Mangled.Set.empty
    else
      match Procdesc.load closure_name with
      | None ->
          Mangled.Set.empty
      | Some pdesc ->
          let ancestors = Procname.Set.add closure_name ancestors in
          let read =
            fold_exps pdesc ~init:Mangled.Set.empty ~f:(fun acc exp ->
                Exp.program_vars exp
                |> Sequence.fold ~init:acc ~f:(fun acc pvar ->
                    Mangled.Set.add (Pvar.get_name pvar) acc ) )
          in
          let read =
            Procname.Set.fold
              (fun nested acc -> Mangled.Set.union acc (free_names ~ancestors nested))
              (closures_of pdesc) read
          in
          let bound =
            List.map (Procdesc.get_formals pdesc) ~f:fst3
            @ List.map (Procdesc.get_locals pdesc) ~f:(fun {ProcAttributes.name} -> name)
            @ List.map (Procdesc.get_captured pdesc) ~f:(fun {CapturedVar.pvar} ->
                Pvar.get_name pvar )
          in
          Mangled.Set.diff read (Mangled.Set.of_list bound)
  in
  Procname.Set.fold
    (fun closure acc -> Mangled.Set.union acc (free_names ~ancestors:Procname.Set.empty closure))
    (closures_of proc_desc) Mangled.Set.empty


let checker {IntraproceduralAnalysis.proc_desc; err_log} =
  let passed_by_ref_invariant_map = get_passed_by_ref_invariant_map proc_desc in
  let cfg = CFG.from_pdesc proc_desc in
  let invariant_map =
    CheckerAnalyzer.exec_cfg cfg
      (proc_desc, passed_by_ref_invariant_map)
      ~initial:ExtendedDomain.bottom
  in
  (* Integer negation uses the captured target widths: unsigned values wrap, while signed
     overflow stays unknown. *)
  let integer_widths =
    lazy (IntegerWidths.load (Procdesc.get_attributes proc_desc).translation_unit)
  in
  let integer_constant_of_type typ constant =
    match typ.Typ.desc with
    | Tint IBool ->
        Some (if IntLit.iszero constant then IntLit.zero else IntLit.one)
    | Tint kind ->
        let open IOption.Let_syntax in
        let* widths = Lazy.force integer_widths in
        if Typ.ikind_is_unsigned kind then
          let bits = IntegerWidths.width_of_ikind widths kind in
          Some (IntLit.of_big_int (Z.extract (IntLit.to_big_int constant) 0 bits))
        else
          let lower, upper = IntegerWidths.range_of_ikind widths kind in
          let value = IntLit.to_big_int constant in
          Option.some_if (Z.leq lower value && Z.leq value upper) constant
    | _ ->
        None
  in
  let rec integer_constant = function
    | Exp.Const (Cint constant) ->
        Some constant
    | Exp.Cast (typ, exp) ->
        Option.bind (integer_constant exp) ~f:(integer_constant_of_type typ)
    | Exp.UnOp (Neg, exp, Some ({Typ.desc= Tint kind} as typ)) when not (Typ.equal_ikind kind IBool)
      ->
        let open IOption.Let_syntax in
        let* constant = integer_constant exp in
        let* constant = integer_constant_of_type typ constant in
        integer_constant_of_type typ (IntLit.neg constant)
    | _ ->
        None
  in
  let rec contains_negation = function
    | Exp.Cast (_, exp) ->
        contains_negation exp
    | Exp.UnOp (Neg, _, _) ->
        true
    | _ ->
        false
  in
  (* we don't want to report in harmless cases like int i = 0; if (...) { i = ... } else { i = ... }
     that create an intentional dead store as an attempt to imitate default value semantics.
     use dead stores to a "sentinel" value as a heuristic for ignoring this case *)
  let rec is_sentinel_exp = function
    | Exp.Cast (_, e) ->
        is_sentinel_exp e
    | Exp.Const (Cint i) ->
        IntLitSet.mem i ignored_constants
    | Exp.Const (Cfloat f) -> (
      match Z.of_float f with
      | z ->
          IntLitSet.mem (IntLit.of_big_int z) ignored_constants
      | exception Z.Overflow ->
          false )
    | _ ->
        false
  in
  let is_sentinel_exp ~typ exp =
    (* The stored type also matters when a conversion is implicit in the SIL store. *)
    is_sentinel_exp exp
    || contains_negation exp
       && Option.exists
            (Option.bind (integer_constant exp) ~f:(integer_constant_of_type typ))
            ~f:(fun constant -> IntLitSet.mem constant ignored_constants)
  in
  let rec is_scope_guard = function
    | {Typ.desc= Tstruct name} ->
        QualifiedCppName.Match.match_qualifiers matcher_scope_guard (Typ.Name.qual_name name)
    | {Typ.desc= Tptr (typ, _)} ->
        is_scope_guard typ
    | _ ->
        false
  in
  let locals = Procdesc.get_locals proc_desc in
  let find_local pvar =
    List.find locals ~f:(fun local_data ->
        Mangled.equal (Pvar.get_name pvar) local_data.ProcAttributes.name )
  in
  let is_constexpr_or_unused pvar =
    let name = Pvar.get_name pvar in
    List.existsi (Procdesc.get_formals proc_desc) ~f:(fun index (formal, _, _) ->
        Mangled.equal name formal
        && List.mem (Procdesc.get_attributes proc_desc).unused_formals index ~equal:Int.equal )
    || find_local pvar
       |> Option.exists ~f:(fun local ->
           local.ProcAttributes.is_constexpr || local.ProcAttributes.is_declared_unused )
  in
  let names_read_by_closures = lazy (names_read_by_closures proc_desc) in
  let is_reference_only_empty_default_construction pvar pname actuals =
    match actuals with
    | [_] ->
        Option.exists (find_local pvar) ~f:(fun local ->
            local.ProcAttributes.is_empty_pod_reference_only )
        && Option.exists (Attributes.load pname) ~f:(fun attrs ->
            attrs.ProcAttributes.is_cpp_implicit )
    | _ ->
        false
  in
  let is_const_read_by_closure pvar =
    find_local pvar
    |> Option.exists ~f:(fun local -> Typ.is_const local.ProcAttributes.typ.Typ.quals)
    && Mangled.Set.mem (Pvar.get_name pvar) (Lazy.force names_read_by_closures)
  in
  let is_block_listed pvar =
    match Config.liveness_block_list_var_regex with
    | Some r ->
        Str.string_match r (Pvar.to_string pvar) 0
    | None ->
        false
  in
  let should_report pvar typ live_vars passed_by_ref_vars =
    Procdesc.is_non_structured_binding_local_or_formal proc_desc pvar
    && not
         ( Pvar.is_frontend_tmp pvar || Pvar.is_return pvar || Pvar.is_global pvar
         || Pvar.is_artificial pvar || is_constexpr_or_unused pvar
         || VarSet.mem (Var.of_pvar pvar) passed_by_ref_vars
         || ExtendedDomain.mem (Var.of_pvar pvar) live_vars
         || is_scope_guard typ
         || Procdesc.has_modify_in_block_attr proc_desc pvar
         || Mangled.is_underscore (Pvar.get_name pvar)
         || is_block_listed pvar || is_const_read_by_closure pvar )
  in
  let log_report pvar typ loc =
    let message = F.asprintf "The value written to `%a` is never used" Pvar.pp_value pvar in
    let trace_message = F.asprintf "Write of unused value (type `%a`)" (Typ.pp_full Pp.text) typ in
    let ltr = [Errlog.make_trace_element 0 loc trace_message []] in
    Reporting.log_issue proc_desc err_log ~loc ~ltr Liveness IssueType.dead_store message
  in
  let report_dead_store live_vars passed_by_ref_vars = function
    | Sil.Store {e1= Lvar pvar; typ; e2= rhs_exp; loc}
      when should_report pvar typ live_vars passed_by_ref_vars && not (is_sentinel_exp ~typ rhs_exp)
      ->
        log_report pvar typ loc
    | Sil.Call (_, e_fun, ((arg, typ) :: _ as actuals), loc, _) -> (
      match (Exp.ignore_cast e_fun, Exp.ignore_cast arg) with
      | Exp.Const (Cfun (Procname.ObjC_Cpp _ as pname)), Exp.Lvar pvar
        when Procname.is_constructor pname
             && should_report pvar typ live_vars passed_by_ref_vars
             && (not (is_reference_only_empty_default_construction pvar pname actuals))
             && ( Procname.is_objc_method pname
                || is_copy_or_move_constructor_call actuals
                || constructor_writes_only_to_this ~depth:3 pname ) ->
          log_report pvar typ loc
      | _, _ ->
          () )
    | _ ->
        ()
  in
  let report_on_node node =
    let passed_by_ref_vars =
      match PassedByRefAnalyzer.extract_post (alias_node_id node) passed_by_ref_invariant_map with
      | Some ((_, passed_by_ref), _) ->
          passed_by_ref
      | None ->
          VarSet.empty
    in
    let node_id = CFG.Node.id node in
    Instrs.iter (CFG.instrs node) ~f:(fun instr ->
        match CheckerAnalyzer.extract_pre node_id invariant_map with
        | Some live_vars ->
            report_dead_store live_vars passed_by_ref_vars instr
        | None ->
            () )
  in
  Container.iter cfg ~fold:CFG.fold_nodes ~f:report_on_node
