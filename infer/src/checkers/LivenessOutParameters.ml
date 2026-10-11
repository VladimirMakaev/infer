(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

open! IStd

(* A switch need not cover every value of its enum type. A successful decoder can nevertheless
   guarantee a finite set of output values. Keep that proof local to liveness: it must not remove
   the frontend's legal fallback edges, or assume that a failed decoder assigned its output. *)
module CFG = ProcCfg.OneInstrPerNode (ProcCfg.Exceptional)
module IdMap = PrettyPrintable.MakePPMap (Ident)
module Constants = Stdlib.Set.Make (IntLit)

let instr node : Sil.instr option =
  Instrs.fold (CFG.instrs node) ~init:None ~f:(fun _ instr -> Some instr)


let predecessors cfg node = CFG.fold_preds cfg node ~init:[] ~f:(fun nodes node -> node :: nodes)

let direct_loads pdesc =
  Procdesc.fold_instrs pdesc ~init:IdMap.empty ~f:(fun loads _ (instr : Sil.instr) ->
      match instr with Sil.Load {id; e= Lvar pvar} -> IdMap.add id pvar loads | _ -> loads )


(* Cycles, missing writes, unknown calls after a write, casts, and more than eight values all
   decline the proof. The backwards walk stops at the last assignment on each successful path. *)
let changes_formal pdesc formal =
  Procdesc.fold_instrs pdesc ~init:false ~f:(fun changed _ (instr : Sil.instr) ->
      changed
      ||
      match instr with
      | Sil.Store {e1= Lvar pvar} when Pvar.equal pvar formal ->
          true
      | Call (_, _, actuals, _, _) ->
          List.exists actuals ~f:(fun (exp, _) ->
              match exp with Exp.Lvar pvar -> Pvar.equal pvar formal | _ -> false )
      | Store {e2} ->
          Exp.program_vars e2 |> Sequence.exists ~f:(Pvar.equal formal)
      | _ ->
          false )


let successful_values pdesc formal output_typ disjoint_formals =
  let loads = direct_loads pdesc in
  let disjoint_formals =
    Pvar.Set.filter (fun pvar -> not (changes_formal pdesc pvar)) disjoint_formals
  in
  if changes_formal pdesc formal then None
  else
    let cfg = CFG.from_pdesc pdesc in
    let clean_return_tail node =
      let budget = ref 128 in
      let rec walk seen node =
        decr budget ;
        if !budget < 0 || CFG.Node.IdSet.mem (CFG.Node.id node) seen then false
        else
          let seen = CFG.Node.IdSet.add (CFG.Node.id node) seen in
          match instr node with
          | Some (Sil.Call _) ->
              false
          | Some (Store {e1= Lvar pvar}) when Pvar.is_return pvar ->
              false
          | Some (Store {e1= Lvar _}) ->
              CFG.fold_succs cfg node ~init:true ~f:(fun valid next -> valid && walk seen next)
          | Some (Store _) ->
              false
          | _ ->
              CFG.fold_succs cfg node ~init:true ~f:(fun valid next -> valid && walk seen next)
      in
      CFG.fold_succs cfg node ~init:true ~f:(fun valid next ->
          valid && walk CFG.Node.IdSet.empty next )
    in
    let budget = ref 512 in
    let rec walk seen node =
      decr budget ;
      if !budget < 0 || CFG.Node.IdSet.mem (CFG.Node.id node) seen then None
      else
        let seen = CFG.Node.IdSet.add (CFG.Node.id node) seen in
        match instr node with
        | Some (Sil.Store {e1= Var id; e2= Const (Cint value); typ})
          when Typ.equal typ output_typ
               && Option.exists (IdMap.find_opt id loads) ~f:(Pvar.equal formal) ->
            Some (Constants.singleton value)
        | Some (Store {e1= Lvar _}) ->
            walk_predecessors seen node
        | Some (Store {e1= Var id})
          when Option.exists (IdMap.find_opt id loads) ~f:(fun formal ->
                   Pvar.Set.mem formal disjoint_formals ) ->
            walk_predecessors seen node
        | Some (Store _) ->
            None
        | Some (Call (_, Const (Cfun callee), _, _, _))
          when Procname.equal callee BuiltinDecl.__infer_skip ->
            walk_predecessors seen node
        | Some (Call _) ->
            None
        | _ ->
            walk_predecessors seen node
    and walk_predecessors seen node =
      match predecessors cfg node with
      | [] ->
          None
      | nodes ->
          List.fold nodes ~init:(Some Constants.empty) ~f:(fun values node ->
              let open IOption.Let_syntax in
              let* values = values in
              let* more = walk seen node in
              let values = Constants.union values more in
              Option.some_if (Constants.cardinal values <= 8) values )
    in
    let returns =
      CFG.fold_nodes cfg ~init:[] ~f:(fun returns node ->
          match instr node with
          | Some (Sil.Store {e1= Lvar pvar; e2}) when Pvar.is_return pvar ->
              (node, e2) :: returns
          | _ ->
              returns )
    in
    if List.is_empty returns then None
    else
      List.fold returns ~init:(Some Constants.empty) ~f:(fun values (node, exp) ->
          let open IOption.Let_syntax in
          let* values = values in
          match exp with
          | Exp.Const (Cint zero) when IntLit.iszero zero ->
              Some values
          | Exp.Const (Cint one) when IntLit.isone one && clean_return_tail node ->
              let* more = walk_predecessors CFG.Node.IdSet.empty node in
              let values = Constants.union values more in
              Option.some_if (Constants.cardinal values <= 8) values
          | _ ->
              None )
      |> Option.filter ~f:(fun values -> not (Constants.is_empty values))


let comparison (exp : Exp.t) =
  match exp with
  | Exp.BinOp (Binop.Eq, Var id, Const (Cint value)) ->
      Some (id, value, true)
  | BinOp (Ne, Var id, Const (Cint value)) | UnOp (LNot, BinOp (Eq, Var id, Const (Cint value)), _)
    ->
      Some (id, value, false)
  | _ ->
      None


let infeasible_switches pdesc =
  let cfg = CFG.from_pdesc pdesc in
  let switches =
    CFG.fold_nodes cfg ~init:[] ~f:(fun switches node ->
        match instr node with
        | Some (Sil.Prune (exp, _, _, Sil.Ik_switch)) -> (
          match comparison exp with
          | Some comparison ->
              (node, comparison) :: switches
          | None ->
              switches )
        | _ ->
            switches )
  in
  if List.is_empty switches then CFG.Node.IdSet.empty
  else
    let loads = direct_loads pdesc in
    let calls =
      CFG.fold_nodes cfg ~init:IdMap.empty ~f:(fun calls node ->
          match instr node with
          | Some (Sil.Call ((id, {Typ.desc= Tint IBool}), Const (Cfun callee), actuals, _, _)) ->
              IdMap.add id (node, callee, actuals) calls
          | _ ->
              calls )
    in
    let contract callee actuals pvar =
      let open IOption.Let_syntax in
      let* callee_desc = Procdesc.load callee in
      List.find_mapi actuals ~f:(fun index (actual, typ) ->
          match (actual, typ.Typ.desc) with
          | Exp.Lvar actual_pvar, Tptr (({desc= Tint _} as output_typ), _)
            when Pvar.equal actual_pvar pvar
                 && List.exists (Procdesc.get_locals pdesc) ~f:(fun local ->
                     Mangled.equal local.ProcAttributes.name (Pvar.get_name pvar)
                     && Typ.equal local.ProcAttributes.typ output_typ ) ->
              let* formal, _, _ = List.nth (Procdesc.get_formals callee_desc) index in
              let disjoint_formals =
                List.foldi actuals ~init:Pvar.Set.empty ~f:(fun other disjoint (actual, _) ->
                    match actual with
                    | Exp.Lvar other_pvar when not (Pvar.equal pvar other_pvar) -> (
                      match List.nth (Procdesc.get_formals callee_desc) other with
                      | Some (formal, _, _) ->
                          Pvar.Set.add (Pvar.mk formal callee) disjoint
                      | None ->
                          disjoint )
                    | _ ->
                        disjoint )
              in
              successful_values callee_desc (Pvar.mk formal callee) output_typ disjoint_formals
          | _ ->
              None )
    in
    let unchanged pvar stop start =
      let budget = ref 128 in
      let rec walk seen node =
        decr budget ;
        if !budget < 0 then false
        else if CFG.Node.IdSet.mem (CFG.Node.id node) seen then false
        else if CFG.Node.equal node stop then true
        else
          let seen = CFG.Node.IdSet.add (CFG.Node.id node) seen in
          match instr node with
          | Some (Sil.Call _) ->
              false
          | Some (Store {e1= Lvar written}) when Pvar.equal written pvar ->
              false
          | Some (Store {e1= Lvar _}) ->
              previous seen node
          | Some (Store _) ->
              false
          | _ ->
              previous seen node
      and previous seen node =
        let nodes = predecessors cfg node in
        (not (List.is_empty nodes)) && List.for_all nodes ~f:(walk seen)
      in
      previous CFG.Node.IdSet.empty start
    in
    List.fold switches ~init:CFG.Node.IdSet.empty ~f:(fun infeasible (node, (id, value, equal)) ->
        match IdMap.find_opt id loads with
        | None ->
            infeasible
        | Some pvar ->
            let budget = ref 128 in
            let rec walk seen predicates node =
              decr budget ;
              if !budget < 0 || CFG.Node.IdSet.mem (CFG.Node.id node) seen then false
              else
                let seen = CFG.Node.IdSet.add (CFG.Node.id node) seen in
                match instr node with
                | Some (Sil.Prune (Var return_id, _, _, _)) -> (
                  match IdMap.find_opt return_id calls with
                  | Some (call_node, callee, actuals) when unchanged pvar call_node node ->
                      Option.exists (contract callee actuals pvar) ~f:(fun values ->
                          Constants.for_all
                            (fun candidate ->
                              List.exists predicates ~f:(fun (value, equal) ->
                                  Bool.equal equal (not (IntLit.equal candidate value)) ) )
                            values )
                  | _ ->
                      previous seen predicates node )
                | Some (Prune (exp, _, _, Sil.Ik_switch)) -> (
                  match comparison exp with
                  | Some (other, value, equal) when Ident.equal id other ->
                      previous seen ((value, equal) :: predicates) node
                  | _ ->
                      previous seen predicates node )
                | Some (Call _) ->
                    false
                | Some (Store {e1= Lvar written}) when Pvar.equal written pvar ->
                    false
                | Some (Store {e1= Lvar _}) ->
                    previous seen predicates node
                | Some (Store _) ->
                    false
                | _ ->
                    previous seen predicates node
            and previous seen predicates node =
              let nodes = predecessors cfg node in
              (not (List.is_empty nodes)) && List.for_all nodes ~f:(walk seen predicates)
            in
            if previous CFG.Node.IdSet.empty [(value, equal)] node then
              CFG.Node.IdSet.add (CFG.Node.id node) infeasible
            else infeasible )
