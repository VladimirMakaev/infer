(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

open! IStd

(** Type of functions to report issues to the error_log in a spec. *)

type 'a log_result =
     ?loc_instantiated:Location.t
  -> ?ltr:Errlog.loc_trace
  -> ?extras:Jsonbug_t.extra
  -> ?autofix:Jsonbug_t.autofix list
  -> ?suggestion:string
  -> Checker.t
  -> IssueType.t
  -> string
  -> 'a

type log_t = unit log_result

val log_issue : Procdesc.t -> Errlog.t -> loc:Location.t -> log_t
(** Add an issue to the given error log. *)

val log_issue_recorded : Procdesc.t -> Errlog.t -> loc:Location.t -> bool log_result
(** [log_issue], saying whether the issue was really recorded. It is not when the issue is filtered
    out or suppressed, and not when the error log already holds one with the same message: the log
    keys on the message, so two findings that read the same become one entry. A caller that
    remembers what it has reported, to avoid reporting it twice, must remember only what this
    returns [true] for. Remembering an issue that never reached the log makes the next report of it
    look like a repeat, and the finding is lost. *)

val log_issue_external :
     Procname.t
  -> issue_log:IssueLog.t
  -> ?severity_override:IssueType.severity
  -> loc:Location.t
  -> ltr:Errlog.loc_trace
  -> ?access:string
  -> ?extras:Jsonbug_t.extra
  -> ?autofix:Jsonbug_t.autofix list
  -> ?suggestion:string
  -> Checker.t
  -> IssueType.t
  -> string
  -> IssueLog.t
(** Log an issue to the error log in [IssueLog] associated with the given procname. *)

val is_suppressed :
  ?field_name:Fieldname.t option -> Tenv.t -> ProcAttributes.t -> IssueType.t -> bool
(** should an issue report be suppressed due to a [@SuppressLint("issue")] annotation? *)
