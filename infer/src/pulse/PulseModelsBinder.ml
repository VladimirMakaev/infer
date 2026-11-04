(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

open! IStd
open PulseModelsImport
module DSL = PulseModelsDSL

(** [AStatus], the C status object behind [ndk::ScopedAStatus].

    The [AStatus_*] functions live in a shared library, so a caller only ever sees their
    declarations and every status a factory returns is an unconstrained pointer. A status check is
    then undecided, and both of its branches stay feasible: code that returns early when the call
    failed looks to the analysis like code that may also carry on, and whatever the early return was
    protecting is reported against a path that cannot happen.

    A status is three independent integers - the exception, the service-specific code and the
    transport status - because the accessors read them separately and a caller may branch on any of
    them. They are only known for a status this module built: one that arrived from somewhere else
    keeps unknown values and stays undecided, which is the honest answer. *)
module NdkStatus = struct
  let exception_field =
    Fieldname.make PulseOperations.pulse_model_type "__infer_model_astatus_exception"


  let service_specific_field =
    Fieldname.make PulseOperations.pulse_model_type "__infer_model_astatus_service_specific"


  let status_field = Fieldname.make PulseOperations.pulse_model_type "__infer_model_astatus_status"

  (* from binder_status.h; the analysis only needs the ones a factory stores or a caller compares
     against *)
  let ex_none = 0

  let ex_service_specific = -8

  let ex_transaction_failed = -129

  let status_ok = 0

  (* The accessors take the status by pointer and really do dereference it, so a null status is a
     null dereference in the program. This module does not say so: it reads with [no_access], which
     records no validity requirement, because making the accessors dereference their argument would
     have these models ADD null-dereference reports on every unguarded [getExceptionCode()] - the
     inline C++ wrapper guards only [isOk()]. That is a different change from removing the
     false positives this module is for, and it belongs in its own commit with its own corpus
     evidence. *)
  let read_field status field =
    let open DSL.Syntax in
    load_access ~no_access:true ~deref:false status (FieldAccess field)


  let write_status ~exception_code ~service_specific ~status =
    let open DSL.Syntax in
    let* ptr = fresh () in
    (* every factory is a [new AStatus(...)], which does not return null *)
    prune_ne_zero ptr
    @@> write_field ~ref:ptr exception_field exception_code
    @@> write_field ~ref:ptr service_specific_field service_specific
    @@> write_field ~ref:ptr status_field status
    @@> assign_ret ptr


  let new_ok : model =
    let open DSL.Syntax in
    start_named_model "AStatus_newOk"
    @@ fun () ->
    let* none = int ex_none in
    let* zero = int status_ok in
    write_status ~exception_code:none ~service_specific:zero ~status:zero


  (* the exception is stored as given: a caller that builds a status with one code and then reads
     the code back gets the same one *)
  let from_exception_code exception_code : model =
    let open DSL.Syntax in
    start_named_model "AStatus_fromExceptionCode"
    @@ fun () ->
    let exception_code = to_aval exception_code in
    let* zero = int status_ok in
    write_status ~exception_code ~service_specific:zero ~status:zero


  let from_service_specific_error service_specific : model =
    let open DSL.Syntax in
    start_named_model "AStatus_fromServiceSpecificError"
    @@ fun () ->
    let service_specific = to_aval service_specific in
    let* ex = int ex_service_specific in
    let* zero = int status_ok in
    write_status ~exception_code:ex ~service_specific ~status:zero


  (* [STATUS_OK] is an ok status; any other transport status is a failed transaction that keeps the
     status so the caller can tell which failure it was *)
  let from_status status : model =
    let open DSL.Syntax in
    start_named_model "AStatus_fromStatus"
    @@ fun () ->
    let status = to_aval status in
    let* zero = int status_ok in
    disj
      [ ( prune_eq_int status (IntLit.of_int status_ok)
        @@>
        let* none = int ex_none in
        write_status ~exception_code:none ~service_specific:zero ~status:zero )
      ; ( prune_ne_int status (IntLit.of_int status_ok)
        @@>
        let* failed = int ex_transaction_failed in
        write_status ~exception_code:failed ~service_specific:zero ~status ) ]


  (* [AStatus_isOk] is "the exception is [EX_NONE]" *)
  let is_ok status : model =
    let open DSL.Syntax in
    start_named_model "AStatus_isOk"
    @@ fun () ->
    let status = to_aval status in
    let* exception_code = read_field status exception_field in
    let* none = int ex_none in
    let* is_none = binop Binop.Eq exception_code none in
    assign_ret is_none


  let get_field ~desc field status : model =
    let open DSL.Syntax in
    start_named_model desc
    @@ fun () ->
    let status = to_aval status in
    read_field status field >>= assign_ret


  (* Releasing the status frees it; it does not touch anything else the caller can see. Without a
     model this is an unknown call, which may write through everything it can reach - and because
     [~ScopedAStatus] releases on every scope exit, that unknown call lands in ordinary code all
     the time. Saying "no effect" keeps those facts.

     It deliberately does NOT invalidate the status: that would be the more faithful model, but it
     would make this commit add use-after-free reports rather than remove false positives. *)
  let delete : model =
    let open DSL.Syntax in
    start_named_model "AStatus_delete" @@ fun () -> ret ()
end

(** [android::binder::Status], the C++ status the AIDL C++ backend returns.

    Unlike {!NdkStatus} this class has real fields and inline accessors - [isOk] is
    [mException == EX_NONE] and the callers read [mException] and [mErrorCode] directly - so there
    is nothing to model on the reading side. What is missing is the writing side: every factory and
    the private two-argument constructor are defined in libbinder, so a status arrives with its
    fields unwritten and every check on it decides nothing.

    These models therefore write the class's OWN fields rather than fields of their own invention,
    and the existing inline accessors then produce the right answers by themselves. [mMessage] is
    left alone: the analysis has no use for the text, and writing a string field a factory may not
    have set would claim more than is known. *)
module BinderStatus = struct
  let status_type =
    Typ.CppClass
      { name= QualifiedCppName.of_qual_string "android::binder::Status"
      ; template_spec_info= NoTemplate
      ; is_union= false }


  let exception_field = Fieldname.make status_type "mException"

  let error_code_field = Fieldname.make status_type "mErrorCode"

  (* Status::Exception *)
  let ex_none = 0

  let ex_service_specific = -8

  let ex_transaction_failed = -129

  (* android::status_t; FAILED_TRANSACTION is UNKNOWN_ERROR + 2 *)
  let status_ok = 0

  let failed_transaction = -2147483646

  (* A factory returns by value, so the frontend hands it the object to fill as the last actual and
     these models write through that, the way [std::make_error_code] is modelled. *)
  let write ~exception_code ~error_code result =
    let open DSL.Syntax in
    store_field ~ref:result exception_field exception_code
    @@> store_field ~ref:result error_code_field error_code


  let write_consts ~exception_code ~error_code result =
    let open DSL.Syntax in
    let* ex = int exception_code in
    let* err = int error_code in
    write ~exception_code:ex ~error_code:err result


  let ok result : model =
    let open DSL.Syntax in
    start_named_model "android::binder::Status::ok"
    @@ fun () ->
    let result = to_aval result in
    write_consts ~exception_code:ex_none ~error_code:status_ok result


  (* libbinder special-cases exactly one code: a status built as [EX_TRANSACTION_FAILED] carries
     [FAILED_TRANSACTION] as its error code, so [transactionError()] hands that back rather than
     [OK]. Every other code stores no error code. *)
  let from_exception_code exception_code result : model =
    let open DSL.Syntax in
    start_named_model "android::binder::Status::fromExceptionCode"
    @@ fun () ->
    let exception_code = to_aval exception_code in
    let result = to_aval result in
    disj
      [ ( prune_eq_int exception_code (IntLit.of_int ex_transaction_failed)
        @@>
        let* failed = int failed_transaction in
        write ~exception_code ~error_code:failed result )
      ; ( prune_ne_int exception_code (IntLit.of_int ex_transaction_failed)
        @@>
        let* zero = int status_ok in
        write ~exception_code ~error_code:zero result ) ]


  let from_service_specific_error error_code result : model =
    let open DSL.Syntax in
    start_named_model "android::binder::Status::fromServiceSpecificError"
    @@ fun () ->
    let error_code = to_aval error_code in
    let result = to_aval result in
    let* ex = int ex_service_specific in
    write ~exception_code:ex ~error_code result


  (* [setFromStatusT] is two-way and keeps the status verbatim as the error code:
     [mException = status == NO_ERROR ? EX_NONE : EX_TRANSACTION_FAILED; mErrorCode = status].
     There is no case for any particular status value, so every non-OK status - including
     [UNEXPECTED_NULL] - is a failed transaction that still hands its status back through
     [transactionError()]. *)
  let from_status_t status result : model =
    let open DSL.Syntax in
    start_named_model "android::binder::Status::fromStatusT"
    @@ fun () ->
    let status = to_aval status in
    let result = to_aval result in
    disj
      [ ( prune_eq_int status (IntLit.of_int status_ok)
        @@>
        let* none = int ex_none in
        write ~exception_code:none ~error_code:status result )
      ; ( prune_ne_int status (IntLit.of_int status_ok)
        @@>
        let* failed = int ex_transaction_failed in
        write ~exception_code:failed ~error_code:status result ) ]
end

(** [android::hardware::details::return_status], the base of the HIDL [Return<T>].

    A [Return<T>] carries a transport status that the caller is required to look at. Its destructor
    enforces that: an unchecked return whose transport failed calls [LOG(FATAL)] and the process
    dies, so code placed after the destructor runs only when the call succeeded or was checked.
    Converting a [Return<T>] to its value is the same bargain - it aborts when the transport failed.

    The destructor, [assertOk], [onValueRetrieval] and the move assignment all live in libhidlbase,
    so Infer sees only their declarations and the abort is invisible: every status check stays
    undecided and the code the abort was protecting is reported against a path that cannot happen.
    These models supply the missing aborts by making the dying path unreachable.

    Nothing on the reading side needs a model. [isOk], [isOkUnchecked] and [isDeadObject] are inline
    in the header, so they already read [mStatus] and write [mCheckedStatus] themselves; these
    models therefore read the class's OWN fields and agree with the inline accessors by construction
    rather than by coincidence. *)
module HidlReturn = struct
  let return_status_type =
    Typ.CppClass
      { name= QualifiedCppName.of_qual_string "android::hardware::details::return_status"
      ; template_spec_info= NoTemplate
      ; is_union= false }


  let status_type =
    Typ.CppClass
      { name= QualifiedCppName.of_qual_string "android::hardware::Status"
      ; template_spec_info= NoTemplate
      ; is_union= false }


  let checked_field = Fieldname.make return_status_type "mCheckedStatus"

  let status_field = Fieldname.make return_status_type "mStatus"

  let exception_field = Fieldname.make status_type "mException"

  let ex_none = 0

  (* [mStatus] is an embedded object rather than a pointer, so reaching the exception is a field
     access that is NOT dereferenced followed by one that is - exactly the shape the inline
     [isOkUnchecked] produces. Reading with [no_access] records no validity requirement: the
     receiver of a destructor is live, and demanding more here would make these models add reports
     instead of removing them. *)
  let read_exception this =
    let open DSL.Syntax in
    let* status = load_access ~no_access:true ~deref:false this (FieldAccess status_field) in
    load_access ~no_access:true status (FieldAccess exception_field)


  let read_checked this =
    let open DSL.Syntax in
    load_access ~no_access:true this (FieldAccess checked_field)


  (* [LOG(FATAL)] does not return, so the failing branch simply does not exist for whatever follows *)
  let abort_unless_ok this =
    let open DSL.Syntax in
    let* exception_code = read_exception this in
    disj
      [ prune_eq_int exception_code (IntLit.of_int ex_none) @@> ret ()
      ; prune_ne_int exception_code (IntLit.of_int ex_none) @@> unreachable ]


  (* [~return_status] aborts only when the status was never checked AND the transport failed, so a
     caller that did check keeps both branches - that is what stops this from deciding everything *)
  let destructor this : model =
    let open DSL.Syntax in
    start_named_model "android::hardware::details::return_status::~return_status"
    @@ fun () ->
    let this = to_aval this in
    let* checked = read_checked this in
    disj [prune_ne_zero checked @@> ret (); prune_eq_zero checked @@> abort_unless_ok this]


  let assert_ok ~desc this : model =
    let open DSL.Syntax in
    start_named_model desc
    @@ fun () ->
    let this = to_aval this in
    abort_unless_ok this
end

let matchers : matcher list =
  let open ProcnameDispatcher.Call in
  [ -"AStatus_newOk" <>--> NdkStatus.new_ok
  ; -"AStatus_fromExceptionCode" <>$ capt_arg_payload $--> NdkStatus.from_exception_code
  ; -"AStatus_fromExceptionCodeWithMessage"
    <>$ capt_arg_payload $+ any_arg $--> NdkStatus.from_exception_code
  ; -"AStatus_fromServiceSpecificError"
    <>$ capt_arg_payload $--> NdkStatus.from_service_specific_error
  ; -"AStatus_fromServiceSpecificErrorWithMessage"
    <>$ capt_arg_payload $+ any_arg $--> NdkStatus.from_service_specific_error
  ; -"AStatus_fromStatus" <>$ capt_arg_payload $--> NdkStatus.from_status
  ; -"AStatus_isOk" <>$ capt_arg_payload $--> NdkStatus.is_ok
  ; -"AStatus_getExceptionCode" <>$ capt_arg_payload
    $--> NdkStatus.get_field ~desc:"AStatus_getExceptionCode" NdkStatus.exception_field
  ; -"AStatus_getServiceSpecificError"
    <>$ capt_arg_payload
    $--> NdkStatus.get_field ~desc:"AStatus_getServiceSpecificError"
           NdkStatus.service_specific_field
  ; -"AStatus_getStatus" <>$ capt_arg_payload
    $--> NdkStatus.get_field ~desc:"AStatus_getStatus" NdkStatus.status_field
  ; -"AStatus_delete" <>$ any_arg $--> NdkStatus.delete
    (* the object a factory returns by value is the last actual *)
  ; -"android" &:: "binder" &:: "Status" &:: "ok" <>$ capt_arg_payload $--> BinderStatus.ok
  ; -"android" &:: "binder" &:: "Status" &:: "fromExceptionCode" <>$ capt_arg_payload
    $+ capt_arg_payload $--> BinderStatus.from_exception_code
  ; -"android" &:: "binder" &:: "Status" &:: "fromExceptionCode" <>$ capt_arg_payload $+ any_arg
    $+ capt_arg_payload $--> BinderStatus.from_exception_code
  ; -"android" &:: "binder" &:: "Status" &:: "fromServiceSpecificError" <>$ capt_arg_payload
    $+ capt_arg_payload $--> BinderStatus.from_service_specific_error
  ; -"android" &:: "binder" &:: "Status" &:: "fromServiceSpecificError" <>$ capt_arg_payload
    $+ any_arg $+ capt_arg_payload $--> BinderStatus.from_service_specific_error
  ; -"android" &:: "binder" &:: "Status" &:: "fromStatusT" <>$ capt_arg_payload $+ capt_arg_payload
    $--> BinderStatus.from_status_t
    (* the destructor Infer actually calls is the inner one it splits out *)
  ; -"android" &:: "hardware" &:: "details" &:: "return_status"
    &:: "__infer_inner_destructor_~return_status" <>$ capt_arg_payload
    $+...$--> HidlReturn.destructor
  ; -"android" &:: "hardware" &:: "details" &:: "return_status" &:: "~return_status"
    <>$ capt_arg_payload $+...$--> HidlReturn.destructor
  ; -"android" &:: "hardware" &:: "details" &:: "return_status" &:: "assertOk" <>$ capt_arg_payload
    $+...$--> HidlReturn.assert_ok ~desc:"android::hardware::details::return_status::assertOk"
  ; -"android" &:: "hardware" &:: "details" &:: "return_status" &:: "onValueRetrieval"
    <>$ capt_arg_payload
    $+...$--> HidlReturn.assert_ok
                ~desc:"android::hardware::details::return_status::onValueRetrieval" ]
