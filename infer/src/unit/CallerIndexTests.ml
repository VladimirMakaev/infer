(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

open! IStd
open OUnit2

(* The construction rules of the caller table, tested without a database. They are the part that
   has to agree exactly with the scan the table replaces, and a disagreement here would show up in
   the analysis only as a report that quietly stopped appearing. *)

let pname name = Procname.from_string_c_fun name

let uid name = Procname.to_unique_id (pname name)

let add t ~keep_callee ~caller ~callees =
  CallerIndex.Edges.add t ~keep_callee ~caller_uid:(uid caller) ~callees:(List.map callees ~f:pname)


let keep_all _ = true

let build ?(keep_callee = keep_all) edges =
  List.fold edges ~init:CallerIndex.Edges.empty ~f:(fun t (caller, callees) ->
      add t ~keep_callee ~caller ~callees )


let assert_rows ~expect t =
  let expect =
    List.map expect ~f:(fun (callee, callers) -> (uid callee, List.map callers ~f:uid))
  in
  let got = CallerIndex.Edges.rows t in
  let pp fmt rows =
    Format.fprintf fmt "@[<v>%a@]"
      (Format.pp_print_list (fun fmt (callee, callers) ->
           Format.fprintf fmt "%s <- [%s]" callee (String.concat ~sep:"; " callers) ) )
      rows
  in
  let sort = List.sort ~compare:[%compare: string * string list] in
  assert_equal
    ~pp_diff:(fun fmt (a, b) -> Format.fprintf fmt "@\nexpected %a@\ngot %a@\n" pp a pp b)
    (sort expect) (sort got)


let tests =
  "caller_index_tests"
  >::: [ ( "one caller one callee"
         >:: fun _ -> assert_rows ~expect:[("b", ["a"])] (build [("a", ["b"])]) )
       ; ( "two callers of the same callee are both recorded"
         >:: fun _ -> assert_rows ~expect:[("c", ["a"; "b"])] (build [("a", ["c"]); ("b", ["c"])])
         )
       ; ("a self-call is not an edge" >:: fun _ -> assert_rows ~expect:[] (build [("a", ["a"])]))
       ; ( "a self-call is dropped without dropping the caller's other edges"
         >:: fun _ -> assert_rows ~expect:[("b", ["a"])] (build [("a", ["a"; "b"])]) )
       ; ( "a caller calling the same callee twice appears once"
         >:: fun _ -> assert_rows ~expect:[("b", ["a"])] (build [("a", ["b"; "b"])]) )
       ; ( "the same caller row seen in both procedure tables appears once"
         >:: fun _ -> assert_rows ~expect:[("b", ["a"])] (build [("a", ["b"]); ("a", ["b"])]) )
       ; ( "keep_callee excludes a callee entirely"
         >:: fun _ ->
         let keep_callee p = String.equal (Procname.to_string p) "keep" in
         assert_rows ~expect:[("keep", ["a"])] (build ~keep_callee [("a", ["keep"; "drop"])]) )
       ; ( "a callee with no caller left after filtering has no row"
         >:: fun _ ->
         let keep_callee _ = false in
         assert_rows ~expect:[] (build ~keep_callee [("a", ["b"])]) )
       ; ( "edges are counted per distinct caller, not per call"
         >:: fun _ ->
         let t = build [("a", ["c"; "c"]); ("b", ["c"]); ("c", ["c"])] in
         assert_equal ~printer:string_of_int 2 (CallerIndex.Edges.count_edges t) )
       ; ( "rows are stable under the order the procedures are scanned in"
         >:: fun _ ->
         let one = build [("a", ["c"]); ("b", ["c"]); ("a", ["d"])] in
         let other = build [("a", ["d"]); ("b", ["c"]); ("a", ["c"])] in
         assert_equal
           ~printer:(fun rows -> Format.asprintf "%d rows" (List.length rows))
           (CallerIndex.Edges.rows one) (CallerIndex.Edges.rows other) ) ]
