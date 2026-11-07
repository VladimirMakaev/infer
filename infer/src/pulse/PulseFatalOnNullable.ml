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
   every procedure containing an assert satisfies that. Five questions must all be answered
   yes, and each one is what rejects a specific shape the suite contains:

   - the failing call happened HERE: a value that failed in a callee is the callee's report;
   - the failure DECIDED this exit: the path that ended pinned that call's value to null;
   - no fatal exit happens WITHOUT it: a procedure that also crashes for an unrelated reason is
     not evidence about this value, and reporting it would blame the wrong call;
   - the null is NOT HANDLED: no path that pinned the same value to null, and nothing else
     this exit did not, returned normally. A branch the procedure both dies on and returns
     from was not decided by the null;
   - the success side SURVIVES: if nothing survives then the procedure ends the program
     regardless, and the null return is not what kills it.

   All but the second earn their place by rejecting something. Dropping the first gives every
   transitive caller its own copy of one report, each one carrying the callee's line number
   against the caller's file; dropping the third turns a procedure that handles null and then
   checks an unrelated argument into a report; dropping the fourth reports every graceful
   branch whose logger can throw; dropping the fifth means any always-aborting helper that
   happens to receive a failure value reports. *)

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


(* The locations this procedure's own instructions occupy.

   A report's FILE is not taken from its location. [JsonReports] prints the source file of the
   procedure the report is attached to, and the line from the diagnostic. A diagnostic whose
   location is in another procedure therefore does not merely point somewhere unhelpful: it
   prints that procedure's line against this procedure's file, and the line it lands on is
   usually not even code -- a comment, an [#include]. Two of the three reports in the repro for
   this were a comment line.

   The locations are those of instructions rather than of nodes, because that is where the
   origin's own location comes from: the call instruction the nullable model ran on. *)
let own_locations proc_desc =
  Procdesc.fold_instrs proc_desc ~init:Location.Map.empty ~f:(fun acc _node instr ->
      Location.Map.add (Sil.location_of_instr instr) () acc )


let reportable_origins proc_desc posts =
  let fatal_exits = fatal_exits_of posts in
  if List.is_empty fatal_exits then OriginMap.empty
  else
    let own_locs = own_locations proc_desc in
    (* Does any path survive this procedure at all?

       This is deliberately NOT "some surviving path still has the value non-null". That question
       cannot be asked here: a path that took the non-null branch has no further use for the
       pointer, [exit_scope] and [remove_vars] drop it, and the attribute goes with it. Measured
       on all nine fatal shapes: the surviving post answers "I have never heard of this call". A
       clause phrased that way is not conservative, it is unsatisfiable, and it would have made
       the rule report nothing while every test still passed.

       [handled_as_this_exit_did] below asks the opposite question -- did a surviving path pin the
       value to NULL -- and that one is answerable, because a branch guarded on the null keeps
       the value reachable for exactly as long as it is the branch condition. Same attribute,
       same state, different side of the test. *)
    let some_path_survives =
      List.exists posts ~f:(fun (exec_state, _path) ->
          match (exec_state : ExecutionDomain.t) with ContinueProgram _ -> true | _ -> false )
    in
    (* Which failures did a path pin to null and then RETURN with?

       A procedure that both dies and returns on the same null was not killed by the null. The
       shape this rejects is the common one in the field: the null branch handles the failure --
       logs and returns, or creates the missing object -- and a callee on that branch has its own
       [[noreturn]] error path whose condition is nothing to do with the null
       (fmt::detail::throw_format_error behind a malformed format string,
       absl::ThrowStdLengthError behind a container's length check). Pulse finds that exit
       reachable only under the null, because that is the only branch the call sits on, and
       without this clause the rule blames the null for it. Measured: on a three-report
       reduction 2 of the 3 are that shape, and on one real project it was 133 of 239 locations.

       The cost is a real false negative, and it is the fixture named for it: a null branch that
       recovers on one sub-condition and crashes on another no longer reports. Nothing here can
       separate that from the logger case -- both are "the null branch splits, one side dies" --
       and the field sample says the logger case is the overwhelming majority.

       [ExceptionRaised] does not count as returning: the procedure did not hand an answer back,
       and whether the caller survives is the caller's question, asked again there. *)
    let surviving_null_sets =
      List.filter_map posts ~f:(fun (exec_state, _path) ->
          match (exec_state : ExecutionDomain.t) with
          | ContinueProgram astate ->
              Some (nullable_returns (PulseFailureOrigins.pinned_null astate))
          | ExceptionRaised _ | Stopped _ ->
              None )
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
    (* Was THIS failure the one that was handled?

       Asking only "did some surviving path have this value null" is too coarse, and the gap is
       not academic. With two failure values obtained before either is checked, the path that
       returns because the FIRST is null also carries the second as null -- the second's check
       was never reached on that path, so nothing constrained it. A procedure-wide set then
       contains both, and the second value's genuine fatal goes unreported. That is a silent
       false negative, it grows with the length of the nullable list, and it is not the single-
       origin cost this rule knowingly pays.

       So the surviving path has to be the same branch as the fatal, not merely a path on which
       the value happened to be null. The test is the SAME SET of failures: a surviving path
       that pins exactly what the fatal path pinned differs from it only in things this rule
       does not track -- the format check inside the logger, the length check inside the
       container -- and is the same handling of the same failure. A path that pins a different
       set went somewhere else, because some other value being null is what sent it there.

       Equality rather than containment, and the difference is a shape worth keeping. A crash
       that needs TWO failures at once -- [if (a) { if (b) die(); log(); return; }] -- has a
       fatal pinning both and a surviving path pinning only the first. Containment would call
       the first one handled on the strength of that path and report neither value; equality
       asks for a surviving path that failed in the same way, finds none, and reports both.
       Containment loses the whole crash, not a detail of it.

       The containment form was committed first, justified by a worry that equality would let
       the logger false positives back in. Measured, it does not: a throwing helper always
       leaves a surviving twin on the same branch with an IDENTICAL pinned set, because the
       helper's non-throwing path falls through to the same return. Equality and containment
       agree on every fixture in this file; the conjunctive crash is where they part. *)
    let handled_as_this_exit_did origin exit_origins =
      List.exists surviving_null_sets ~f:(fun surviving ->
          OriginSet.mem origin surviving && OriginSet.equal surviving exit_origins )
    in
    (* Did the failing call happen HERE?

       A failure value outlives the procedure that produced it: stored in a field, returned, or
       simply still reachable, it travels into every caller, and the attribute travels with it.
       The exit does too, because a caller whose callee ends the program ends with it. So a
       procedure that merely calls something that checks a null of its own satisfies every other
       condition here, and before this clause each of those callers got its own copy of one
       report -- transitively, so a chain of four produced four.

       The callee is analysed on its own and reports there, which is the one place where the
       call, the crash and the file all agree. A caller's copy adds nothing and is mislocated by
       construction, for the reason spelled out at [own_locations].

       This does not silence the shape where the caller supplies the failing value -- a null
       passed to a helper that checks it. There the origin is the caller's own call, the helper
       contributes only the exit, and the report stays where the failing call is. That is the
       one caller-side report worth making, and it is the one that is correctly placed. *)
    let origin_is_local ({location} : Attribute.FailureOrigin.t) =
      Location.Map.mem location own_locs
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
                && origin_is_local origin
                && (not (reachable_without_failing origin location))
                && (not (handled_as_this_exit_did origin origins))
                && some_path_survives
              then OriginMap.add origin (location, fatal_callee) acc
              else acc )
            origins acc )


let report_errors ({InterproceduralAnalysis.proc_desc} as analysis_data) posts =
  if IssueType.pulse_fatal_on_nullable_return.enabled then
    reportable_origins proc_desc posts
    |> OriginMap.iter (fun origin (exit_location, fatal_callee) ->
        PulseReport.report analysis_data ~is_suppressed:false ~latent:false
          (Diagnostic.FatalOnNullableReturn {origin; exit_location; fatal_callee}) )
