(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

open! IStd
open PulseBasicInterface
open PulseDomainInterface

(* A function on the nullable-return list returns null to say "this failed". When the caller's
   answer to that failure is to end the program -- a CHECK, an INVARIANT, an assert -- the crash
   is reachable in normal operation, and nothing reports it today: the disjunct simply stops.

   The rule is NOT "a fatal exit is reachable on a path where a failure value exists". Nearly
   every procedure containing an assert satisfies that. Three questions must all be answered
   yes, and each one is what rejects a specific shape the suite contains:

   - the failure DECIDED this exit: the path that ended pinned that call's value to null;
   - no fatal exit happens WITHOUT it: a procedure that also crashes for an unrelated reason is
     not evidence about this value, and reporting it would blame the wrong call;
   - the success side SURVIVES: if no surviving path has the value non-null then the procedure
     ends the program regardless, and the null return is not what kills it.

   The last two are the interesting ones. Dropping the second turns a procedure that handles
   null and then checks an unrelated argument into a report; dropping the third means any
   always-aborting helper that happens to receive a failure value reports. *)

(* Values are per-disjunct, so a value number cannot be followed from one post to another. The
   origin can: it is the callee plus the call site, the same record in every disjunct that call
   reaches. All cross-disjunct reasoning below is keyed on it. *)
module OriginSet = Attribute.FailureOriginSet
module OriginMap = PrettyPrintable.MakePPMap (Attribute.FailureOrigin)

(* Only [NullableReturn] origins are read. [FailureOutcome] exists in the type so the general
   failure contract can be added later without disturbing the attribute's rank, and nothing
   constructs one yet, so handling it here would be code no test can reach. *)
let nullable_returns origins =
  OriginSet.filter
    (fun ({source} : Attribute.FailureOrigin.t) ->
      match source with NullableReturn -> true | FailureOutcome -> false )
    origins


(* fatal exits as (state, where it ended, what ended it, what was null when it did). An inline
   record cannot escape its constructor, so the fields are taken apart here. *)
let fatal_exits_of posts =
  List.filter_map posts ~f:(fun (exec_state, _path) ->
      match (exec_state : ExecutionDomain.t) with
      | Stopped (ExitProgram {astate; exit= {kind= Fatal; location; fatal_callee; failure_origins}})
        ->
          Some (astate, location, fatal_callee, nullable_returns failure_origins)
      | _ ->
          None )


let reportable_origins posts =
  let fatal_exits = fatal_exits_of posts in
  if List.is_empty fatal_exits then OriginMap.empty
  else
    (* Does any path survive this procedure at all?

       This is deliberately NOT "some surviving path still has the value non-null", which is what
       it looks like it should be. By the time a path reaches the exit node its locals are gone --
       [exit_scope] and [remove_vars] run first -- and the checked pointer is dead, so its
       attribute has been discarded and every surviving post answers "I have never heard of this
       call". Measured: on all nine fatal shapes the surviving post reports an empty origin set.
       A clause phrased that way is not conservative, it is unsatisfiable, and it would have made
       the rule report nothing while every test still passed.

       The work that clause was there to do is already done by [reachable_without_failing]
       below, which
       is the real content of "the success side cannot reach that fatal": if the value being
       non-null could reach a fatal exit, that exit would be one that does not pin it to null.
       What remains is the weaker, and computable, question of whether the procedure has any way
       out other than dying -- which is what rejects a helper that aborts unconditionally. *)
    let some_path_survives =
      List.exists posts ~f:(fun (exec_state, _path) ->
          match (exec_state : ExecutionDomain.t) with ContinueProgram _ -> true | _ -> false )
    in
    (* Can THIS crash site be reached with the value non-null? If some path ends the program at
       the same place without the value being null, the crash is not about this call, and saying
       it is would blame the wrong one.

       Keyed on the exit LOCATION, not on the procedure. A global version -- "no fatal anywhere
       happens without this origin" -- suppresses an ordinary shape: assert on an unrelated
       argument, then check the failure value, and the unrelated crash, which does happen with
       the value non-null, hides the one that does not. That is a false negative on the report
       the rule exists to make. Per site, the unrelated crash is excluded on its own merits and
       the real one still reports.

       An exit that does not mention the origin at all counts as reaching it non-null: a path
       that ended while saying nothing about this call is not evidence that the call is to blame.
       Counting it loses reports rather than inventing them. *)
    let reachable_without_failing origin location =
      List.exists fatal_exits ~f:(fun (_, other_location, _, origins) ->
          Location.equal location other_location && not (OriginSet.mem origin origins) )
    in
    List.fold fatal_exits ~init:OriginMap.empty
      ~f:(fun acc (astate, location, fatal_callee, origins) ->
        (* Manifest only. A latent fatal needs something of the caller to be reachable, and this
           issue type is registered as never-latent: [Diagnostic.get_issue_type] dies on a latent
           one rather than inventing a _LATENT variant nobody asked for. *)
        if not (PulseArithmetic.is_manifest astate) then acc
        else
          OriginSet.fold
            (fun origin acc ->
              if
                (not (OriginMap.mem origin acc))
                && (not (reachable_without_failing origin location))
                && some_path_survives
              then OriginMap.add origin (location, fatal_callee) acc
              else acc )
            origins acc )


let report_errors analysis_data posts =
  if IssueType.pulse_fatal_on_nullable_return.enabled then
    reportable_origins posts
    |> OriginMap.iter (fun origin (exit_location, fatal_callee) ->
        PulseReport.report analysis_data ~is_suppressed:false ~latent:false
          (Diagnostic.FatalOnNullableReturn {origin; exit_location; fatal_callee}) )
