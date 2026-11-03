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
  ; -"AStatus_delete" <>$ any_arg $--> NdkStatus.delete ]
