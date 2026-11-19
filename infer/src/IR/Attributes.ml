(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

open! IStd

let find =
  let select_statement db =
    Database.register_statement db "SELECT proc_attributes FROM procedures WHERE proc_uid = :k"
  in
  let select_statement_adb = select_statement AnalysisDatabase in
  let select_statement_cdb = select_statement CaptureDatabase in
  fun proc_uid ->
    let run_query select_stmt =
      Database.with_registered_statement select_stmt ~f:(fun db select_stmt ->
          Sqlite3.bind select_stmt 1 (Sqlite3.Data.TEXT proc_uid)
          |> SqliteUtils.check_result_code db ~log:"find bind proc name" ;
          SqliteUtils.result_single_column_option ~finalize:false ~log:"Attributes.find" db
            select_stmt
          |> Option.map ~f:ProcAttributes.SQLite.deserialize )
    in
    (* Since the procedure table can be updated in the analysis phase,
       we need to query both databases, analysisdb first *)
    IOption.if_none_evalopt
      ~f:(fun () -> run_query select_statement_cdb)
      (run_query select_statement_adb)


let iter_procs_and_callees =
  let select_statement db =
    Database.register_statement db
      "SELECT proc_uid, proc_attributes, callees FROM procedures WHERE cfg IS NOT NULL"
  in
  let select_statement_adb = select_statement AnalysisDatabase in
  let select_statement_cdb = select_statement CaptureDatabase in
  fun ?(filter_callees = fun _ -> true) f ->
    (* Unlike {!SyntacticCallGraph.iter_captured_procs_and_callees}, which runs before the
       analysis and so only needs the capture database, this one is callable during the analysis
       and queries both, analysis database first, for the same reason {!find} does: the procedure
       table can be updated in the analysis phase. *)
    let seen = IString.Hash.create 1024 in
    let iter_db select_stmt =
      Database.with_registered_statement select_stmt ~f:(fun db stmt ->
          SqliteUtils.result_fold_rows db ~log:"Attributes.iter_procs_and_callees" ~finalize:false
            stmt ~init:() ~f:(fun () stmt ->
              (* Ordered by what each column costs to decode. [callees] is a list of names;
                 the attributes are a marshalled record of every formal, annotation and
                 location, so they are decoded only for the rows [filter_callees] keeps.
                 Decoding them for every row of the estate, which is what the obvious order
                 does, is the whole cost of this scan for a caller after a handful of callees. *)
              let callees : Procname.t list =
                Sqlite3.column stmt 2 |> Procname.SQLiteList.deserialize
              in
              if filter_callees callees then
                (* Deduplicated here rather than over every row: the second database's copy of a
                   procedure must be dropped, but [callees] is written at capture and both tables
                   carry the same list, so a uid that passes the filter in one passes in both and
                   this sees every duplicate the whole-table version would. *)
                let uid = Sqlite3.column_text stmt 0 in
                if not (IString.Hash.mem seen uid) then (
                  IString.Hash.add seen uid () ;
                  let proc_name =
                    Sqlite3.column stmt 1 |> ProcAttributes.SQLite.deserialize
                    |> ProcAttributes.get_proc_name
                  in
                  f proc_name callees ) ) )
    in
    iter_db select_statement_adb ;
    iter_db select_statement_cdb


module Cache = Procname.Cache

let load, clear_cache, store, set_lru_limit =
  (* capture DB attribute cache: only keeps positive entries as analysis may add entries *)
  let cache : ProcAttributes.t Cache.t = Cache.create ~name:"attributes" in
  let load_from_uid uid =
    let result = find uid in
    Option.iter result ~f:(fun attrs -> Cache.add cache (ProcAttributes.get_proc_name attrs) attrs) ;
    result
  in
  let load pname =
    Dependencies.record_pname_dep Other pname ;
    match Cache.lookup cache pname with
    | Some _ as result ->
        result
    | None -> (
      match load_from_uid (Procname.to_unique_id pname) with
      | None ->
          MissingDependencies.record_procname pname ;
          None
      | some ->
          some )
  in
  let clear_cache () = Cache.clear cache in
  let store ?check_stored_definition ~proc_desc (attr : ProcAttributes.t) ~analysis =
    if attr.is_defined && Option.is_none proc_desc then
      Logging.die InternalError "Was given DEFINED procedure without procdesc: %a@."
        ProcAttributes.pp attr ;
    if (not attr.is_defined) && Option.is_some proc_desc then
      Logging.die InternalError
        "Was given UNDEFINED procedure WITH procdesc:@\nAttributes:%a@\nProcdesc:%a@."
        ProcAttributes.pp attr (Pp.option Procdesc.pp_signature) proc_desc ;
    let check_stored_definition =
      Option.map check_stored_definition ~f:(fun check stored ->
          check (ProcAttributes.SQLite.deserialize stored) )
    in
    Procdesc.replace_attributes_degrading ~check_stored_definition ~proc_desc attr ~analysis ;
    Cache.remove cache attr.proc_name
  in
  let set_lru_limit ~lru_limit = Cache.set_lru_mode cache ~lru_limit in
  (load, clear_cache, store, set_lru_limit)


let load_exn pname = Option.value_exn (load pname)

let load_formal_types pname =
  match load pname with Some {formals} -> List.map formals ~f:snd3 | None -> []


let is_no_return pname = match load pname with Some {is_no_return} -> is_no_return | _ -> false
