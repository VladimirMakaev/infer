(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

open! IStd
open OUnit2

let class_name info ~is_union =
  Typ.Name.Cpp.from_qual_name info ~is_union (QualifiedCppName.of_qual_string "CacheProbe")


let qual_string name = QualifiedCppName.to_qual_string (Typ.Name.qual_name name)

let swift_info plain_name =
  let name = SwiftClassName.of_string ~plain_name "same_mangled_name" in
  Typ.Template {mangled= None; args= [TType (Typ.mk_struct (SwiftClass name))]}


let tests =
  "template_names"
  >::: [ ( "equal_but_differently_printed_arguments"
         >:: fun _ ->
         HashNormalizer.reset_all_normalizers () ;
         let a = swift_info "A" and b = swift_info "B" in
         assert_bool "fixture has equal template info" (Typ.equal_template_spec_info a b) ;
         assert_equal "CacheProbe<A>" (qual_string (class_name a ~is_union:false)) ;
         assert_equal "CacheProbe<B>" (qual_string (class_name b ~is_union:false)) )
       ; ( "suffix_repeat_reset_and_distinct_result"
         >:: fun _ ->
         HashNormalizer.reset_all_normalizers () ;
         let info = Typ.Template {mangled= None; args= [TInt 7L]} in
         let name = class_name info ~is_union:false in
         let first = Typ.Name.qual_name name and second = Typ.Name.qual_name name in
         assert_equal "CacheProbe<7>" (QualifiedCppName.to_qual_string first) ;
         assert_equal "CacheProbe<7>" (QualifiedCppName.to_qual_string second) ;
         assert_bool "qualified result remains freshly constructed" (not (phys_equal first second)) ;
         HashNormalizer.reset_all_normalizers () ;
         assert_equal "CacheProbe<7>" (qual_string name) )
       ; ( "class_bucket_collisions_keep_unequal_arguments"
         >:: fun _ ->
         HashNormalizer.reset_all_normalizers () ;
         let name n = class_name (Typ.Template {mangled= None; args= [TInt n]}) ~is_union:false in
         let a = name 1L and b = name 2L and another_a = name 1L in
         let a' = Typ.Name.hash_normalize a in
         let b' = Typ.Name.hash_normalize b in
         let another_a' = Typ.Name.hash_normalize another_a in
         assert_bool "different arguments remain different" (not (Typ.Name.equal a' b')) ;
         assert_bool "equal arguments share canonical representative" (phys_equal a' another_a') )
       ; ( "class_ignored_field_representative_is_preserved"
         >:: fun _ ->
         HashNormalizer.reset_all_normalizers () ;
         let info = Typ.Template {mangled= None; args= [TInt 1L]} in
         let a = class_name info ~is_union:false and b = class_name info ~is_union:true in
         assert_bool "union flag is equality-ignored" (Typ.Name.equal a b) ;
         let a' = Typ.Name.hash_normalize a in
         let b' = Typ.Name.hash_normalize b in
         assert_bool "first representative still wins" (phys_equal a' b') )
       ; ( "unmangled_hash_cache_churn_preserves_representatives"
         >:: fun _ ->
         HashNormalizer.reset_all_normalizers () ;
         let name n ~is_union =
           class_name (Typ.Template {mangled= None; args= [TInt n]}) ~is_union
         in
         let first = Typ.Name.hash_normalize (name 7L ~is_union:false) in
         for n = 100 to 2200 do
           ignore (Typ.Name.hash_normalize (name (Int64.of_int n) ~is_union:false))
         done ;
         let repeated = Typ.Name.hash_normalize (name 7L ~is_union:true) in
         assert_bool "cache eviction preserves original bucket and first representative"
           (phys_equal first repeated) ;
         HashNormalizer.reset_all_normalizers () ;
         let after_reset = Typ.Name.hash_normalize (name 7L ~is_union:false) in
         assert_bool "normalizer reset preserves the value" (Typ.Name.equal first after_reset) )
       ; ( "unmangled_hash_cache_releases_memo_only_keys"
         >:: fun _ ->
         HashNormalizer.reset_all_normalizers () ;
         let name n ~is_union =
           class_name (Typ.Template {mangled= None; args= [TInt n]}) ~is_union
         in
         ignore (Typ.Name.hash_normalize (name 7L ~is_union:false)) ;
         let weak_alias () =
           let weak = Stdlib.Weak.create 1 in
           let alias = name 7L ~is_union:true in
           Stdlib.Weak.set weak 0 (Some alias) ;
           (* Equality ignores the union flag, so the semantic table returns the
              existing representative and never retains this distinct alias. *)
           ignore (Typ.Name.hash_normalize alias) ;
           weak
         in
         let retained weak = Option.is_some (Stdlib.Weak.get weak 0) in
         let evicted = weak_alias () in
         Stdlib.Gc.full_major () ;
         assert_bool "memo-only key is initially retained" (retained evicted) ;
         for n = 100 to 2200 do
           ignore (Typ.Name.hash_normalize (name (Int64.of_int n) ~is_union:false))
         done ;
         Stdlib.Gc.full_major () ;
         assert_bool "eviction releases memo-only key" (not (retained evicted)) ;
         let reset = weak_alias () in
         Stdlib.Gc.full_major () ;
         assert_bool "new memo-only key is initially retained" (retained reset) ;
         HashNormalizer.reset_all_normalizers () ;
         Stdlib.Gc.full_major () ;
         assert_bool "reset releases memo-only key" (not (retained reset)) )
       ; ( "instruction_digest_collisions_keep_distinct_types"
         >:: fun _ ->
         HashNormalizer.reset_all_normalizers () ;
         let id = Ident.create_fresh Ident.knormal in
         let typ n =
           Typ.mk_struct
             (class_name
                (Typ.Template {mangled= Some "same_instantiation"; args= [TInt n]})
                ~is_union:false )
         in
         let store typ =
           Sil.Store
             {e1= Exp.Var id; typ; e2= Exp.Const (Const.Cint IntLit.zero); loc= Location.dummy}
         in
         let a = typ 1L and b = typ 2L in
         assert_equal (Typ.normalizer_hash a) (Typ.normalizer_hash b) ;
         let first = Sil.hash_normalize_instr (store a) in
         let distinct = Sil.hash_normalize_instr (store b) in
         assert_bool "full equality still distinguishes digest collisions"
           (not (Sil.equal_instr first distinct)) ;
         let repeated = Sil.hash_normalize_instr (store (typ 1L)) in
         assert_bool "first representative remains canonical" (phys_equal first repeated) ;
         assert_equal (Stdlib.Marshal.to_string first []) (Stdlib.Marshal.to_string repeated []) )
       ; ( "instruction_digests_preserve_equality_ignored_display_fields"
         >:: fun _ ->
         HashNormalizer.reset_all_normalizers () ;
         let id = Ident.create_fresh Ident.knormal in
         let typ plain = Typ.mk_struct (class_name (swift_info plain) ~is_union:false) in
         let a = typ "A" and b = typ "B" in
         assert_bool "display-only difference is equality-ignored" (Typ.equal a b) ;
         assert_equal (Typ.normalizer_hash a) (Typ.normalizer_hash b) ;
         let load typ = Sil.Load {id; e= Exp.Var id; typ; loc= Location.dummy} in
         let first = Sil.hash_normalize_instr (load a) in
         let repeated = Sil.hash_normalize_instr (load b) in
         assert_bool "lookup preserves the original representative" (phys_equal first repeated) ;
         assert_equal (Stdlib.Marshal.to_string first []) (Stdlib.Marshal.to_string repeated []) )
       ; ( "late_call_arguments_preserve_digest_diversity"
         >:: fun _ ->
         HashNormalizer.reset_all_normalizers () ;
         let id = Ident.create_fresh Ident.knormal in
         let typ =
           Typ.mk_struct
             (class_name (Typ.Template {mangled= None; args= [TInt 1L]}) ~is_union:false)
         in
         let callee = Exp.Const (Const.Cfun (Procname.from_string_c_fun "argument_digest_probe")) in
         let call n =
           let args =
             List.init 10 ~f:(fun i ->
                 let value = if Int.equal i 9 then n else 0 in
                 (Exp.Const (Const.Cint (IntLit.of_int value)), StdTyp.int) )
           in
           Sil.Call ((id, typ), callee, args, Location.dummy, CallFlags.default)
         in
         let digests =
           List.init 64 ~f:(fun n -> Sil.normalizer_hash_instr (call n))
           |> List.dedup_and_sort ~compare:Int.compare
         in
         assert_bool "late arguments are not collapsed into a single bucket"
           (List.length digests >= 16) )
       ; ( "method_templates_in_plain_classes_use_compact_digests"
         >:: fun _ ->
         HashNormalizer.reset_all_normalizers () ;
         let id = Ident.create_fresh Ident.knormal in
         let cls = class_name Typ.NoTemplate ~is_union:false in
         let call n =
           let info = Typ.Template {mangled= None; args= [TInt n]} in
           let pname =
             Procname.ObjC_Cpp
               (Procname.ObjC_Cpp.make cls "invoke"
                  (Procname.ObjC_Cpp.CPPMethod (Some "same_method_key")) info [] )
           in
           Sil.Call
             ((id, StdTyp.void), Exp.Const (Const.Cfun pname), [], Location.dummy, CallFlags.default)
         in
         let a = call 1L and b = call 2L in
         assert_bool "method template arguments remain semantically distinct"
           (not (Sil.equal_instr a b)) ;
         assert_equal (Sil.normalizer_hash_instr a) (Sil.normalizer_hash_instr b) ;
         let first = Sil.hash_normalize_instr a and second = Sil.hash_normalize_instr b in
         assert_bool "full equality distinguishes the compact digest collision"
           (not (Sil.equal_instr first second)) ) ]
