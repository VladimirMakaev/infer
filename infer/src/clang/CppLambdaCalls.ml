(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

open! IStd
module F = Format

let is_std_function_function procname =
  let name = F.asprintf "%a" Procname.pp_without_templates procname in
  String.is_prefix name ~prefix:"std::function::function"


let is_lambda_copy_ctor proc_name attributes =
  Procname.is_lambda proc_name && attributes.ProcAttributes.is_cpp_copy_ctor


let is_lambda_move_ctor proc_name attributes =
  Procname.is_lambda proc_name && attributes.ProcAttributes.is_cpp_move_ctor


let is_callee_lambda_copy_or_move_ctor cfg callee_procname =
  match Procname.Hash.find_opt cfg callee_procname with
  | Some procdesc ->
      let callee_attributes = Procdesc.get_attributes procdesc in
      is_lambda_copy_ctor callee_procname callee_attributes
      || is_lambda_move_ctor callee_procname callee_attributes
  | None ->
      false


let updateTemps proc_desc =
  let temp_ids =
    List.filter_map
      ~f:(fun var_data -> var_data.ProcAttributes.tmp_id)
      (Procdesc.get_locals proc_desc)
  in
  Ident.update_name_generator temp_ids ;
  ProcCfg.Normal.fold_nodes proc_desc ~init:() ~f:(fun _ node ->
      let used_ids = Instrs.instrs_get_normal_vars (ProcCfg.Normal.instrs node) in
      Ident.update_name_generator used_ids )


let process_instr ~proc_name proc_desc loc (exp, typ) (acc1, acc2) =
  if Exp.is_cpp_closure exp then (
    let new_var = Pvar.mk_tmp Pvar.materialized_cpp_temporary proc_name in
    let new_var_data : ProcAttributes.var_data = ProcAttributes.default_var_data new_var typ in
    let new_var_exp = Exp.Lvar new_var in
    Procdesc.append_locals proc_desc [new_var_data] ;
    let store_instr = Sil.Store {e1= new_var_exp; typ; e2= exp; loc} in
    ((new_var_exp, typ) :: acc1, store_instr :: acc2) )
  else ((exp, typ) :: acc1, acc2)


(** This does the following replacement:

    f(closure) ---> temp = closure; f(temp)

    because callees, including the operator() of lambdas, expect a pointer to a closure. *)
let replace_lambda_as_arg_calls proc_name proc_desc =
  let add_calls _node _ instr =
    let instrs =
      match (instr : Sil.instr) with
      | Sil.Call (ret_id_typ, Const (Cfun callee_procname), args, loc, flags) ->
          let new_args, new_instrs =
            List.fold_right ~init:([], []) ~f:(process_instr ~proc_name proc_desc loc) args
          in
          List.append new_instrs
            [Sil.Call (ret_id_typ, Const (Cfun callee_procname), new_args, loc, flags)]
      | _ ->
          [instr]
    in
    Array.of_list instrs
  in
  updateTemps proc_desc ;
  ignore
    (Procdesc.replace_instrs_by_using_context proc_desc ~f:add_calls
       ~update_context:(fun () _ -> ())
       ~context_at_node:(fun _ -> ()) )


let is_lambda_type typ =
  match typ.Typ.desc with
  | Typ.Tptr ({desc= Tstruct name}, Pk_pointer) | Tstruct name ->
      Procname.is_lambda_name (Typ.Name.name name)
  | _ ->
      false


(* The copy and move constructors of a closure are removed from the CFG by [remove_lambda_ctor], so
   every call to one has to be rewritten here: otherwise it calls a procedure with no body and the
   closure's captures are lost. The destination is a program variable for [auto g = f;] but a field
   when the closure is stored in a member, as in [function_(std::forward<Fn>(fn))].

   An aggregate that inherits from its closures, as the C++17 overload idiom does, is the one
   destination that must be left alone: each base subobject is constructed at the address of the
   aggregate itself, so turning those constructor calls into stores would leave every store but
   the last writing a value nothing reads. Rewrite a variable only when the variable really is
   the closure. *)
let pointee_name typ =
  match typ.Typ.desc with Typ.Tptr ({desc= Tstruct name}, _) -> Some name | _ -> None


let local_type_name proc_desc pvar =
  List.find_map (Procdesc.get_locals proc_desc) ~f:(fun {ProcAttributes.name; typ} ->
      if Mangled.equal name (Pvar.get_name pvar) then
        match typ.Typ.desc with Typ.Tstruct type_name -> Some type_name | _ -> None
      else None )


let is_closure_destination proc_desc (exp : Exp.t) typ =
  match exp with
  | Lfield _ ->
      true
  | Lvar pvar -> (
    match (pointee_name typ, local_type_name proc_desc pvar) with
    | Some closure_name, Some var_name ->
        Typ.Name.equal closure_name var_name
    | _, _ ->
        (* the destination's type is not a named struct we can compare: leave it rewritten, as
           it was before aggregates of closures were considered *)
        true )
  | _ ->
      false


let swap_copy_ctor_lambda_calls cfg _proc_name proc_desc =
  let add_calls _node _ instr =
    let instrs =
      match (instr : Sil.instr) with
      | Sil.Call
          ( _ret_id_typ
          , Const (Cfun callee_procname)
          , [(destination, typ); (lambda_arg, lambda_arg_typ)]
          , loc
          , _flags )
        when is_closure_destination proc_desc destination typ
             && ( is_callee_lambda_copy_or_move_ctor cfg callee_procname
                || (is_std_function_function callee_procname && is_lambda_type lambda_arg_typ) ) ->
          let new_id = Ident.create_fresh Ident.knormal in
          let load_lambda_instr = Sil.Load {id= new_id; e= lambda_arg; typ= lambda_arg_typ; loc} in
          let store_var_instr = Sil.Store {e1= destination; typ; e2= Exp.Var new_id; loc} in
          [load_lambda_instr; store_var_instr]
      | _ ->
          [instr]
    in
    Array.of_list instrs
  in
  updateTemps proc_desc ;
  ignore
    (Procdesc.replace_instrs_by_using_context proc_desc ~f:add_calls
       ~update_context:(fun () _ -> ())
       ~context_at_node:(fun _ -> ()) )


let remove_lambda_ctor proc_name proc_desc =
  let attributes = Procdesc.get_attributes proc_desc in
  if is_lambda_copy_ctor proc_name attributes || is_lambda_move_ctor proc_name attributes then None
  else Some proc_desc


let process_proc cfg proc_name proc_desc =
  replace_lambda_as_arg_calls proc_name proc_desc ;
  swap_copy_ctor_lambda_calls cfg proc_name proc_desc


let process cfg =
  Ident.NameGenerator.reset () ;
  Procname.Hash.iter (process_proc cfg) cfg ;
  Procname.Hash.filter_map_inplace remove_lambda_ctor cfg
