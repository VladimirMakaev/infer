(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

open! IStd
open OUnit2
module Dom = BO.BufferOverrunDomain
module Env = BO.BufferOverrunOndemandEnv
module Sem = BO.BufferOverrunSemantics
module Bound = BO.Bounds.Bound
module Itv = BO.Itv
module Symb = BO.Symb
module AbsLoc = BO.AbsLoc
module BoUtils = BO.BufferOverrunUtils

let widths =
  IntegerWidths.{char_width= 8; short_width= 16; int_width= 32; long_width= 64; longlong_width= 64}


let interval lower upper = Itv.join (Itv.of_int lower) (Itv.of_int upper)

let assert_interval expected actual =
  assert (Itv.leq ~lhs:actual ~rhs:expected && Itv.leq ~lhs:expected ~rhs:actual)


let test_1 _ =
  let check kind source expected =
    assert_interval
      (Itv.of_big_int (Z.of_string expected))
      (Itv.cast_ikind widths kind (Itv.of_big_int (Z.of_string source)))
  in
  check Typ.ISChar "300" "44" ;
  check Typ.IUChar "300" "44" ;
  check Typ.ISChar "200" "-56" ;
  check Typ.IUInt "-2" "4294967294" ;
  check Typ.IInt "4294967296" "0" ;
  check Typ.IULongLong "18446744073709551619" "3" ;
  check Typ.IBool "0" "0" ;
  check Typ.IBool "2" "1" ;
  check Typ.IBool "-2" "1"


let test_2 _ =
  assert_interval (interval 0 44) (Itv.cast_ikind widths Typ.IUChar (interval 256 300)) ;
  assert_interval (interval 0 255) (Itv.cast_ikind widths Typ.IUChar (interval 255 256)) ;
  assert_interval (interval (-128) 127) (Itv.cast_ikind widths Typ.ISChar (interval 127 128)) ;
  assert_interval (interval 1 1) (Itv.cast_ikind widths Typ.IBool (interval (-5) (-1))) ;
  assert_interval (interval 0 1) (Itv.cast_ikind widths Typ.IBool (interval (-1) 1)) ;
  assert (Itv.is_bottom (Itv.cast_ikind widths Typ.IUChar Itv.bot))


let test_3 _ =
  (* Masking and sign-bit interpretation are independent of Euclidean division.
     Judge each concrete member of an interval, including both wrap endpoints. *)
  for lower = -300 to 300 do
    for span = 0 to 4 do
      let upper = lower + span in
      List.iter [Typ.IUChar; Typ.ISChar] ~f:(fun kind ->
          let result = Itv.cast_ikind widths kind (interval lower upper) in
          for value = lower to upper do
            let bits = value land 255 in
            let expected =
              if Typ.equal_ikind kind Typ.ISChar && bits >= 128 then bits - 256 else bits
            in
            assert (Itv.leq ~lhs:(Itv.of_int expected) ~rhs:result)
          done )
    done
  done


let environment ?(locals = []) instructions =
  let cfg = Cfg.create () in
  let pname = Procname.from_string_c_fun "integer_conversion_unit" in
  let attrs = {(ProcAttributes.default (SourceFile.invalid __FILE__) pname) with locals} in
  let pdesc = Cfg.create_proc_desc cfg attrs in
  let start = Procdesc.create_node pdesc Location.dummy Procdesc.Node.Start_node [] in
  let body =
    Procdesc.create_node pdesc Location.dummy (Procdesc.Node.Stmt_node Skip) instructions
  in
  let exit = Procdesc.create_node pdesc Location.dummy Procdesc.Node.Exit_node [] in
  Procdesc.set_start_node pdesc start ;
  Procdesc.set_exit_node pdesc exit ;
  Procdesc.node_set_succs pdesc start ~normal:[body] ~exn:[] ;
  Procdesc.node_set_succs pdesc body ~normal:[exit] ~exn:[] ;
  Env.mk pdesc (Tenv.create ()) widths


let load id typ =
  let pvar =
    Pvar.mk (Mangled.from_string "source") (Procname.from_string_c_fun "integer_conversion_unit")
  in
  Sil.Load {id; e= Exp.Lvar pvar; typ; loc= Location.dummy}


let test_4 _ =
  let integer = Ident.create_fresh Ident.knormal in
  let pointer = Ident.create_fresh Ident.knormal in
  let conflict = Ident.create_fresh Ident.knormal in
  let missing = Ident.create_fresh Ident.knormal in
  let integer_typ = Typ.mk (Typ.Tint Typ.ILongLong) in
  let pointer_typ = Typ.mk (Typ.Tptr (integer_typ, Typ.Pk_pointer)) in
  let env =
    environment
      [ load integer integer_typ
      ; load pointer pointer_typ
      ; load conflict integer_typ
      ; load conflict pointer_typ ]
  in
  let classify = env.Env.integer_source_of_exp in
  assert (Env.equal_integer_source (classify (Exp.Var integer)) (Env.Integer (Some Typ.ILongLong))) ;
  assert (Env.equal_integer_source (classify (Exp.Var pointer)) Env.NonInteger) ;
  assert (Env.equal_integer_source (classify (Exp.Var conflict)) Env.Unknown) ;
  assert (Env.equal_integer_source (classify (Exp.Var missing)) Env.Unknown) ;
  assert (Env.equal_integer_source (classify (Exp.int (IntLit.of_int 300))) (Env.Integer None))


let test_unknown_source_guards _ =
  let pname = Procname.from_string_c_fun "integer_conversion_unit" in
  let pvar = Pvar.mk (Mangled.from_string "modeled_source") pname in
  let path = Symb.SymbolPath.of_pvar pvar in
  let env = environment [] in
  let check label value =
    let id = Ident.create_fresh Ident.knormal in
    assert_bool "guard fixture must have unknown source kind"
      (Env.equal_integer_source (env.Env.integer_source_of_exp (Exp.Var id)) Env.Unknown) ;
    let mem =
      Dom.Mem.init (fun _ -> None) env |> Dom.Mem.add_stack (AbsLoc.Loc.of_var (Var.of_id id)) value
    in
    let cast = Exp.Cast (Typ.mk (Typ.Tint Typ.IUInt), Exp.Var id) in
    let converted = Sem.eval widths cast mem in
    assert_bool label (phys_equal value converted)
  in
  check "unknown-source bottom was made reachable" Dom.Val.bot ;
  let pointer = Dom.Val.of_loc (AbsLoc.Loc.of_pvar pvar) in
  assert_bool "pointer fixture has no locations"
    (not (AbsLoc.PowLoc.is_bot (Dom.Val.get_all_locs pointer))) ;
  check "unknown-source cast discarded pointer locations" pointer ;
  let array = Dom.Val.of_literal_string widths "model" in
  assert_bool "array fixture has no array block"
    (not (BO.ArrayBlk.is_bot (Dom.Val.get_array_blk array))) ;
  check "unknown-source cast discarded array position and size" array ;
  let non_integer = Dom.Val.of_itv (Itv.of_normal_path ~unsigned:false ~non_int:true path) in
  assert_bool "symbol fixture is not a modeled non-integer"
    (Symb.SymbolSet.exists Symb.Symbol.is_non_int (Itv.get_symbols (Dom.Val.get_itv non_integer))) ;
  check "unknown-source cast discarded non-integer symbols" non_integer


let test_unknown_source_fits _ =
  let env = environment [] in
  List.iter
    [ (Typ.IUChar, interval 0 255, true, interval 0 255)
    ; (Typ.IUChar, interval 0 256, false, interval 0 255)
    ; (Typ.ISChar, interval (-128) 127, true, interval (-128) 127)
    ; (Typ.ISChar, interval (-129) 127, false, interval (-128) 127)
    ; (Typ.IBool, Itv.zero, true, Itv.zero)
    ; (Typ.IBool, Itv.one, true, Itv.one) ]
    ~f:(fun (kind, input, fits, expected) ->
      let id = Ident.create_fresh Ident.knormal in
      assert_bool "fits fixture must have unknown source kind"
        (Env.equal_integer_source (env.Env.integer_source_of_exp (Exp.Var id)) Env.Unknown) ;
      let value = Dom.Val.set_itv_updated_by_addition (Dom.Val.of_itv input) in
      let mem =
        Dom.Mem.init (fun _ -> None) env
        |> Dom.Mem.add_stack (AbsLoc.Loc.of_var (Var.of_id id)) value
      in
      let converted = Sem.eval widths (Exp.Cast (Typ.mk (Typ.Tint kind), Exp.Var id)) mem in
      assert_interval expected (Dom.Val.get_itv converted) ;
      if fits then
        assert_bool "fitting cast lost its value or metadata" (phys_equal value converted) )


let test_5 _ =
  let known = Ident.create_fresh Ident.knormal in
  let unknown = Ident.create_fresh Ident.knormal in
  let env = environment [load known (Typ.mk (Typ.Tint Typ.ILongLong))] in
  let original = Dom.Val.of_int 300 in
  let mem = Dom.Mem.init (fun _ -> None) env in
  let add id mem = Dom.Mem.add_stack (AbsLoc.Loc.of_var (Var.of_id id)) original mem in
  let mem = add known mem |> add unknown in
  let cast id = Exp.Cast (Typ.mk (Typ.Tint Typ.IUChar), Exp.Var id) in
  assert_interval (Itv.of_int 44) (Dom.Val.get_itv (Sem.eval widths (cast known) mem)) ;
  assert_interval (interval 0 255) (Dom.Val.get_itv (Sem.eval widths (cast unknown) mem)) ;
  assert_interval (Itv.of_int 300) (Dom.Val.get_itv (Sem.eval widths (Exp.Var known) mem)) ;
  assert_bool "fixture must exercise the unknown-source branch"
    (Env.equal_integer_source (env.Env.integer_source_of_exp (Exp.Var unknown)) Env.Unknown) ;
  List.iter
    [ (Typ.ISChar, "-128", "127")
    ; (Typ.IUChar, "0", "255")
    ; (Typ.IInt, "300", "300")
    ; (Typ.IUInt, "300", "300")
    ; (Typ.IULongLong, "300", "300")
    ; (Typ.IBool, "0", "1") ]
    ~f:(fun (kind, lower, upper) ->
      let expected =
        Itv.join (Itv.of_big_int (Z.of_string lower)) (Itv.of_big_int (Z.of_string upper))
      in
      let cast = Exp.Cast (Typ.mk (Typ.Tint kind), Exp.Var unknown) in
      assert_interval expected (Dom.Val.get_itv (Sem.eval widths cast mem)) ) ;
  assert_interval (Itv.of_int 300) (Dom.Val.get_itv (Sem.eval widths (Exp.Var unknown) mem))


let test_6 _ =
  let pname = Procname.from_string_c_fun "integer_conversion_unit" in
  let pvar = Pvar.mk (Mangled.from_string "symbol") pname in
  let path = Symb.SymbolPath.of_pvar pvar in
  let source = Dom.Val.of_itv (Itv.of_normal_path ~unsigned:false path) in
  let signed = Dom.Val.cast_integer ~source:Typ.IInt widths Typ.ILongLong source in
  assert (phys_equal source signed) ;
  let source = Dom.Val.of_itv (Itv.of_normal_path ~unsigned:true path) in
  let unsigned = Dom.Val.cast_integer ~source:Typ.IUChar widths Typ.IInt source in
  assert (phys_equal source unsigned)


let test_nonnegative_type_bound _ =
  let value = Dom.Val.of_itv (Itv.set_lb_zero Itv.top) in
  let converted = Dom.Val.cast_integer ~source:Typ.IInt widths Typ.IUInt value in
  assert_bool "non-negative typed value lost its identity" (phys_equal value converted)


let test_boolean_identity _ =
  let negative = Dom.Val.of_int (-1) in
  assert_interval Itv.one (Dom.Val.get_itv (Dom.Val.cast_integer widths Typ.IBool negative)) ;
  assert_interval Itv.one (Dom.Val.get_itv (Dom.Val.cast_boolean (Dom.Val.of_int 2)))


let test_boolean_storage _ =
  let pname = Procname.from_string_c_fun "integer_conversion_unit" in
  let integer = Pvar.mk (Mangled.from_string "integer_storage") pname in
  let boolean = Pvar.mk (Mangled.from_string "boolean_storage") pname in
  let missing = Pvar.mk (Mangled.from_string "unknown_storage") pname in
  let env =
    environment
      ~locals:
        [ ProcAttributes.default_var_data integer (Typ.mk (Typ.Tint Typ.IInt))
        ; ProcAttributes.default_var_data boolean (Typ.mk (Typ.Tint Typ.IBool)) ]
      []
  in
  assert_bool "integer-to-Bool storage conversion was missed"
    (env.Env.bool_load_is_conversion (Exp.Lvar integer)) ;
  assert_bool "ordinary Boolean load was treated as a conversion"
    (not (env.Env.bool_load_is_conversion (Exp.Lvar boolean))) ;
  assert_bool "unknown storage type was guessed"
    (not (env.Env.bool_load_is_conversion (Exp.Lvar missing)))


let test_boolean_aliases _ =
  let pname = Procname.from_string_c_fun "integer_conversion_unit" in
  let integer = Pvar.mk (Mangled.from_string "integer_storage") pname in
  let boolean = Pvar.mk (Mangled.from_string "boolean_storage") pname in
  let env = environment [] in
  let raw = Dom.Mem.init (fun _ -> None) env in
  let int_loc = AbsLoc.Loc.of_pvar integer in
  let bool_loc = AbsLoc.Loc.of_pvar boolean in
  let raw = Dom.Mem.add_heap int_loc (Dom.Val.of_int 2) raw in
  let raw = Dom.Mem.add_heap bool_loc (Dom.Val.of_itv Itv.top) raw in
  let converted = Ident.create_fresh Ident.knormal in
  let ordinary = Ident.create_fresh Ident.knormal in
  let bool_typ = Typ.mk (Typ.Tint Typ.IBool) in
  let load conversion id loc mem =
    BoUtils.Exec.load_locs ~bool_conversion:conversion ~represents_multiple_values:false
      ~modeled_range:None id bool_typ (AbsLoc.PowLoc.singleton loc) mem
  in
  let mem = raw |> load true converted int_loc |> load false ordinary bool_loc in
  let prune id constant mem =
    Sem.Prune.prune Location.dummy widths
      (Exp.BinOp (Binop.Eq, Exp.Var id, Exp.int (IntLit.of_int constant)))
      mem
  in
  let mem = prune converted 1 mem in
  assert_bool "a changed Boolean value was equated to its integer storage"
    (Dom.Mem.is_reachable mem) ;
  assert_interval (Itv.of_int 2) (Dom.Val.get_itv (Dom.Mem.find int_loc mem)) ;
  let mem = prune ordinary 0 mem in
  assert_interval Itv.zero (Dom.Val.get_itv (Dom.Mem.find bool_loc mem))


let test_symbolic_bounds _ =
  let pname = Procname.from_string_c_fun "integer_conversion_unit" in
  let pvar = Pvar.mk (Mangled.from_string "symbolic_source") pname in
  let path = Symb.SymbolPath.of_pvar pvar in
  let symbolic = Itv.of_normal_path ~unsigned:false path in
  let mixed =
    Itv.set_lb
      (Bound.of_normal_path Symb.Symbol.make_onevalue ~unsigned:false path)
      (interval 0 300)
  in
  List.iter [symbolic; mixed; Itv.top] ~f:(fun value ->
      assert_bool "a symbolic or infinite bound was treated as a numeric constant"
        (not (Itv.has_numeric_bounds value)) ;
      List.iter [Typ.IUChar; Typ.IUInt; Typ.IBool] ~f:(fun kind ->
          assert_interval value (Itv.cast_ikind widths kind value) ) ) ;
  assert_bool "a finite interval needs no singleton requirement"
    (Itv.has_numeric_bounds (interval 255 256))


let test_symbolic_values _ =
  let pname = Procname.from_string_c_fun "integer_conversion_unit" in
  let pvar = Pvar.mk (Mangled.from_string "symbolic_source") pname in
  let path = Symb.SymbolPath.of_pvar pvar in
  let value = Dom.Val.of_itv (Itv.of_normal_path ~unsigned:false path) in
  assert_bool "a symbolic conversion dropped the original value and its dependencies"
    (phys_equal value (Dom.Val.cast_integer ~source:Typ.IInt widths Typ.IUInt value)) ;
  assert_bool "a symbolic Boolean conversion dropped the original value"
    (phys_equal value (Dom.Val.cast_boolean value))


let tests =
  "integer_conversions"
  >::: [ "narrowing and bool witnesses" >:: test_1
       ; "convex hull and one wrap segment" >:: test_2
       ; "independent eight-bit oracle" >:: test_3
       ; "source kinds are captured types, not observed numeric values" >:: test_4
       ; "unknown source is top for width; known source converts" >:: test_5
       ; "unknown source preserves bottom and modeled values" >:: test_unknown_source_guards
       ; "unknown source preserves fitting values and widens others" >:: test_unknown_source_fits
       ; "value-preserving widening keeps symbolic values" >:: test_6
       ; "non-negative value plus source type" >:: test_nonnegative_type_bound
       ; "Boolean identity and consumer-typed loads" >:: test_boolean_identity
       ; "ordinary Boolean versus integer storage" >:: test_boolean_storage
       ; "Boolean conversion and ordinary-load aliases" >:: test_boolean_aliases
       ; "symbolic and mixed bounds stay unchanged" >:: test_symbolic_bounds
       ; "symbolic values preserve their dependencies" >:: test_symbolic_values ]
