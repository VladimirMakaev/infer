(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

open! IStd
module L = Logging

(* The ids rather than the names, both in the table and here, because a caller's name is only
   needed for the handful a query returns: decoding the attributes of the whole estate to build
   the table would reintroduce the cost the table exists to remove. *)
module UidList = SqliteUtils.MarshalledDataNOTForComparison (struct
  type t = string list
end)

(* The row that says the table was built. It is needed because "no rows for this callee" and "the
   build never ran" must not look the same: the first is an answer and the second has to fall back
   to the scan. The second is reachable -- the build is skipped when RacerD is off or the switch
   is set -- and without a marker a skipped build would read as an estate in which nothing calls
   anything. *)
let built_marker_uid = ""

module Edges = struct
  type t = IString.Set.t Procname.Map.t

  let empty = Procname.Map.empty

  let add t ~keep_callee ~caller_uid ~callees =
    List.fold callees ~init:t ~f:(fun t callee ->
        (* A self-call is dropped here rather than at lookup time: a method cannot be the caller
           that establishes its own precondition, and dropping it once, at the only place the edge
           is written, keeps the table and the scan it replaces saying the same thing. *)
        if String.equal caller_uid (Procname.to_unique_id callee) || not (keep_callee callee) then t
        else
          Procname.Map.update callee
            (function
              | Some callers ->
                  Some (IString.Set.add caller_uid callers)
              | None ->
                  Some (IString.Set.singleton caller_uid) )
            t )


  let rows t =
    Procname.Map.fold
      (fun callee callers acc -> (Procname.to_unique_id callee, IString.Set.elements callers) :: acc)
      t []


  let count_edges t = Procname.Map.fold (fun _ callers n -> n + IString.Set.cardinal callers) t 0
end

let select_marker_statement =
  Database.register_statement AnalysisDatabase "SELECT 1 FROM callers WHERE callee_uid = :k LIMIT 1"


let select_callers_statement =
  Database.register_statement AnalysisDatabase
    "SELECT caller_uids FROM callers WHERE callee_uid = :k"


let is_built =
  (* Asked once per process: the table either holds for the whole analysis or it never does,
     because nothing writes it after the build has finished.

     Deliberately WITHOUT a handler for the table being absent. A statement is prepared when the
     connection is opened, so a database without this table cannot get this far: it is refused
     earlier, by the run-state check, because the table is part of [Database.schema_hum] and so
     part of the format string a results directory is validated against. Catching here would
     suggest a robustness that the preparation order does not allow, and would hide the one case
     it cannot help with. *)
  lazy
    (Database.with_registered_statement select_marker_statement ~f:(fun db stmt ->
         Sqlite3.bind stmt 1 (Sqlite3.Data.TEXT built_marker_uid)
         |> SqliteUtils.check_result_code db ~log:"CallerIndex bind marker" ;
         SqliteUtils.result_single_column_option ~finalize:false ~log:"CallerIndex marker" db stmt
         |> Option.is_some ) )


let iter_uids_and_callees =
  let select_statement db =
    Database.register_statement db "SELECT proc_uid, callees FROM procedures WHERE cfg IS NOT NULL"
  in
  let select_statement_adb = select_statement Database.AnalysisDatabase in
  let select_statement_cdb = select_statement Database.CaptureDatabase in
  fun f ->
    (* Both tables, analysis first and deduplicated by uid, exactly as {!Attributes.find} and the
       scan this replaces do, so the table answers from the same rows the scan would have read.
       Unlike that scan this never touches [proc_attributes]: the caller is carried as its id. *)
    let seen = IString.Hash.create 1024 in
    let iter_db select_stmt =
      Database.with_registered_statement select_stmt ~f:(fun db stmt ->
          SqliteUtils.result_fold_rows db ~log:"CallerIndex.iter_uids_and_callees" ~finalize:false
            stmt ~init:() ~f:(fun () stmt ->
              let uid = Sqlite3.column_text stmt 0 in
              if not (IString.Hash.mem seen uid) then (
                IString.Hash.add seen uid () ;
                let callees : Procname.t list =
                  Sqlite3.column stmt 1 |> Procname.SQLiteList.deserialize
                in
                f uid callees ) ) )
    in
    iter_db select_statement_adb ;
    iter_db select_statement_cdb


let build ~keep_callee () =
  let edges = ref Edges.empty in
  iter_uids_and_callees (fun caller_uid callees ->
      edges := Edges.add !edges ~keep_callee ~caller_uid ~callees ) ;
  let rows = Edges.rows !edges in
  let edge_count = Edges.count_edges !edges in
  (* dropped before the write, so the map is not alive while the rows go in and is collectable
     well before the analysis forks its workers, whose heaps start as a copy of this one *)
  edges := Edges.empty ;
  (* Through the writer rather than straight at the database: a writer daemon may already own it
     by the time the analysis starts, and a direct write would be racing whoever else holds it.
     The marker row goes in the same transaction as the table, so a reader can never see a
     half-written table that claims to be complete. *)
  DBWriter.store_caller_index
    ~rows:
      ( List.map rows ~f:(fun (callee_uid, caller_uids) ->
            (callee_uid, UidList.serialize caller_uids) )
      @ [(built_marker_uid, UidList.serialize [])] ) ;
  L.debug Analysis Quiet "CallerIndex: %d callees, %d edges@\n" (List.length rows) edge_count


let callers_of callees =
  if not (Lazy.force is_built) then None
  else
    let resolve uid =
      match Attributes.find uid with
      | Some attrs ->
          Some (ProcAttributes.get_proc_name attrs)
      | None ->
          (* the scan could not have produced a caller whose row it could not read either, so
             dropping it keeps the two paths saying the same thing *)
          L.debug Analysis Quiet "CallerIndex: no attributes for caller %s@\n" uid ;
          None
    in
    let lookup callee =
      Database.with_registered_statement select_callers_statement ~f:(fun db stmt ->
          Sqlite3.bind stmt 1 (Sqlite3.Data.TEXT (Procname.to_unique_id callee))
          |> SqliteUtils.check_result_code db ~log:"CallerIndex bind lookup" ;
          SqliteUtils.result_single_column_option ~finalize:false ~log:"CallerIndex lookup" db stmt
          |> Option.map ~f:UidList.deserialize )
    in
    Stats.add_racerd_caller_index_lookups (Procname.Set.cardinal callees) ;
    Some
      (Procname.Set.fold
         (fun callee acc ->
           match lookup callee with
           | None | Some [] ->
               acc
           | Some uids -> (
             match List.filter_map uids ~f:resolve with
             | [] ->
                 acc
             | callers ->
                 Procname.Map.add callee callers acc ) )
         callees Procname.Map.empty )
