#!/bin/bash
# run.sh --infer ROOT [--tier pre-push|nightly] [--outdir DIR] : the inferbo interval
# soundness grid. Content-side runner, called by the harness's pr/itv_check.sh, which
# owns the path condition and the push policy. Runnable by hand with the same arguments.
#
#   Exit 0  the grid ran and every obligation held. Prints "ALL CHECKS PASSED".
#   Exit 1  an obligation failed (an unsound point, or a control that did not fire).
#   Exit 2  the grid could not be run. Nothing is claimed.
#
# WHAT THIS ASSERTS, POSITIVELY. Not "no failures were seen". The trailer carries the
# case count, the reach-control count, and the calibration verdict, because the whole
# failure mode this grid exists to avoid is a clean verdict from a run that never
# reached the operation under test. A report of violations=0 without cases>0 and
# controls=2/2 is not evidence and this script refuses to emit one.
#
# THE TWO CONTROLS, and why both directions are needed.
#   calib.c  is the DETECTOR calibration. c2_feasible and c4_interval_not_set must
#            REPORT and c3_infeasible must stay SILENT. c3 is the one that matters: it
#            is a guard inferbo is supposed to prune, so if c3 reports, the detector
#            has stopped discriminating and every "sound" verdict in the grid below is
#            meaningless. A control that cannot fail validates nothing.
#   reach_*  is the per-quadruple control. Each probe has a sibling with identical
#            operands and op but an unconditional access. A silent probe whose reach
#            sibling is ALSO silent is INCONCLUSIVE, not a finding: the function was
#            skipped. Only silent-probe-with-firing-reach is an unsoundness.
#
# GENERATED GRIDS ARE NOT COMMITTED. grid_signed.c alone is 2.8MB. gen.py, the seed and
# expected/counts.json are committed instead and the grid is regenerated here. The
# nightly tier re-asserts the regenerated manifest against the committed sha256, so a
# change in gen.py's output fails the check instead of silently moving the baseline.
set -u
export LC_ALL=C

me=inferbo-grid
die() { echo "$me: $*" >&2; echo "ITV GRID INCOMPLETE: $*"; exit 2; }

HERE=$(cd "$(dirname "$(readlink -f "$0")")" && pwd)
INFER_ROOT=""; TIER=pre-push; OUT=""
SEED=${ITV_SEED:-20261117}
SAMPLE_PERMILLE=${ITV_SAMPLE_PERMILLE:-20}

while [ $# -gt 0 ]; do
  case $1 in
    --infer)  [ $# -ge 2 ] || die "--infer needs a path"; INFER_ROOT=$2; shift 2 ;;
    --tier)   [ $# -ge 2 ] || die "--tier needs a value"; TIER=$2; shift 2 ;;
    --outdir) [ $# -ge 2 ] || die "--outdir needs a path"; OUT=$2; shift 2 ;;
    *) die "usage: $me --infer ROOT [--tier pre-push|nightly] [--outdir DIR]" ;;
  esac
done
case $TIER in pre-push|nightly) ;; *) die "--tier must be 'pre-push' or 'nightly', not '$TIER'" ;; esac
[ -n "$INFER_ROOT" ] || die "usage: $me --infer ROOT [--tier pre-push|nightly]"

# An INSTALL, not a worktree: this check must never build Infer. The caller supplies a
# prebuilt root, for the plain reason that a check which builds is a check nobody runs.
INFER_BIN=$INFER_ROOT/bin/infer
[ -x "$INFER_BIN" ] || die "no executable infer at $INFER_BIN (this check needs a prebuilt install, it does not build one)"
command -v clang >/dev/null 2>&1 || die "clang is not on PATH; the grid cannot be captured"
command -v python3 >/dev/null 2>&1 || die "python3 is not on PATH"
[ -f "$HERE/expected/counts.json" ] || die "no expected/counts.json beside $HERE; the baseline is missing, so nothing can be judged"
# The watch list is a precondition, not an optional extra. Without it the sampled tier
# silently falls back to a blind 2% sample: measured, deleting the file took the pre-push
# run from 382 cases and 34 known-unsound points to 346 and 0, and still exited 0 saying
# ALL CHECKS PASSED -- a run that looked CLEANER than reality because it had stopped
# looking. Any file this check's coverage depends on has to be refused when absent, the
# same way counts.json already was.
[ -f "$HERE/expected/always_sample.json" ] || die "no expected/always_sample.json beside $HERE; without it the sampled tier would quietly stop generating the pinned cases and report a clean run for having looked at less"

if [ -z "$OUT" ]; then
  OUT=$(mktemp -d -t inferbo-grid-XXXXXX) || die "could not create a work directory"
  trap 'rm -rf "$OUT"' EXIT
fi
mkdir -p "$OUT" || die "could not create $OUT"

if [ "$TIER" = nightly ]; then ARMS="signed unsigned s32 u32"; PERMILLE=1000
else ARMS="s32 u32 unsigned"; PERMILLE=$SAMPLE_PERMILLE; fi

echo "== $me: tier=$TIER arms=[$ARMS] sample=${PERMILLE}permille seed=$SEED"
echo "== $me: infer install $INFER_ROOT"

# The cd is deliberate: it keeps the compiland a bare basename so that the `file` field of
# every report, and therefore every probe name the grid pins, is relative to the fixture
# directory rather than to wherever the runner happened to be invoked from.
#
# -o is equally deliberate. clang defaults the object next to its input, which for the
# COMMITTED fixtures (calib.c, band.c, ...) is the checked-out source tree, so a plain
# pre-push run used to leave verification/inferbo/calib.o behind. $od already names a
# path inside the work directory and the ".log" sibling beside it follows the same
# convention. Both $od and $src are absolute (OUT is either an absolute --outdir or a
# mktemp -d, and $HERE is resolved through readlink -f), so neither survives the cd by
# accident.
run_infer () { # file outdir -> report.json path on stdout
  local src=$1 od=$2
  ( cd "$(dirname "$src")" && "$INFER_BIN" --bufferoverrun-only --no-progress-bar \
      -o "$od" -- clang -c "$(basename "$src")" -o "$od.o" ) >"$od.log" 2>&1
}

# --- 1. the detector calibration. Runs in BOTH tiers and before anything else: if the
#        detector cannot discriminate there is no point running a grid through it.
echo "== $me: calibration (calib.c)"
run_infer "$HERE/calib.c" "$OUT/calib"
[ -f "$OUT/calib/report.json" ] || die "calib.c produced no report.json; the install did not analyse it (see $OUT/calib.log)"
calib_fired=$(python3 -I -c '
import json,sys
print(" ".join(sorted({r["procedure"] for r in json.load(open(sys.argv[1]))})))' "$OUT/calib/report.json") ||
  die "could not read calib report.json"
# ALL FOUR cases in calib.c carry a contract, so all four are asserted. Earlier only c2 and
# c3 were, which left two of the file's own claims unchecked: c1 is the reachability
# control (if an UNCONDITIONAL access does not report, nothing in the grid can be believed,
# and every "silent probe" would be an artefact of the harness rather than of the analysis)
# and c4 pins the domain as an INTERVAL rather than a set of disjuncts (7 is inside [0,10]
# but not concretely achievable; if c4 were silent, "silent" would be ambiguous between
# exclusion and genuine infeasibility and the grid's central inference would not hold).
CALIB_TOTAL=4
controls=0
calib_expect_report () { # name why
  case " $calib_fired " in
    *" $1 "*) controls=$((controls+1)) ;;
    *) echo "CONTROL FAILED: $1 did not report; $2" ;;
  esac
}
calib_expect_report c1_reach \
  "an unconditional access was not flagged, so a silent probe anywhere in the grid says nothing about the analysis"
calib_expect_report c2_feasible \
  "the detector does not fire on a live guard, so every silent verdict below would be meaningless"
calib_expect_report c4_interval_not_set \
  "the domain is not behaving as an interval, so 'silent' is ambiguous between exclusion and genuine infeasibility"
case " $calib_fired " in
  *" c3_infeasible "*)
    echo "CONTROL FAILED: c3_infeasible REPORTED; inferbo is no longer pruning the guard this grid relies on, so every verdict below would be meaningless" ;;
  *) controls=$((controls+1)) ;;
esac
echo "== $me: calibration fired: [$calib_fired] controls=$controls/$CALIB_TOTAL"

# --- 2. the grid arms
cases=0; reaches_fired=0; reaches_total=0; unsound_airtight=0; unsound_impl=0; manifest_drift=""
for arm in $ARMS; do
  # --always-from forces the watched quadruples into the sample: they are the cases most
  # worth looking at, and letting the seed decide made the sampled tier's set comparison
  # vacuous (measured: 0 of u32's 21 pins landed in the 2% sample).
  #
  # IT READS always_sample.json, NOT known_unsound.json, AND THE SEPARATION IS THE POINT.
  # While the two were one file, deleting a pin also deleted the case from the sample, so
  # the quadruple was never generated, never observed, and the deletion passed silently --
  # i.e. the one edit most worth catching, removing a pin to quieten the check, was the
  # one edit this tier could not see. Keeping the watch list separate means unpinning
  # leaves the case in the sample, where it shows up as a new unsound point.
  python3 -I "$HERE/gen.py" "$arm" --sample "$PERMILLE" --seed "$SEED" --outdir "$OUT" \
    --always-from "$HERE/expected/always_sample.json" >"$OUT/gen_$arm.log" 2>&1 ||
    die "gen.py failed for arm $arm (see $OUT/gen_$arm.log)"
  src=$OUT/grid_$arm.c; man=$OUT/manifest_$arm.json
  [ -s "$src" ] && [ -s "$man" ] || die "gen.py produced no grid for arm $arm"

  # The full grid is the one the baseline describes, so only nightly can assert the sha.
  if [ "$PERMILLE" -ge 1000 ]; then
    # Hash AND counts: the hash pins the manifest exactly, the counts make a mismatch
    # readable ("s32: probes 7558 -> 7000" rather than "a hash changed" about megabytes
    # of generated JSON). Asserting both also keeps the counts from being baseline fields
    # that nothing reads, which look like coverage without being any.
    drift=$(python3 -I "$HERE/manifest_check.py" "$HERE/expected/counts.json" "$arm" "$man") ||
      die "manifest_check.py failed for arm $arm"
    [ -z "$drift" ] || manifest_drift="$manifest_drift $arm($drift)"
  fi

  run_infer "$src" "$OUT/out_$arm"
  [ -f "$OUT/out_$arm/report.json" ] ||
    die "arm $arm produced no report.json; the install did not analyse the grid (see $OUT/out_$arm.log)"
  res=$(python3 -I "$HERE/analyse.py" "$man" "$OUT/out_$arm/report.json" "$OUT/unsound_$arm.json") ||
    die "analyse.py failed for arm $arm"
  printf '%s\n' "$res" | sed "s/^/   [$arm] /"
  a=$(python3 -I - "$man" "$OUT/out_$arm/report.json" <<'PY'
import json,sys
man=json.load(open(sys.argv[1])); rep=json.load(open(sys.argv[2]))
fired={r["procedure"] for r in rep}
probes=[(f,m) for f,m in man.items() if m["kind"]=="probe"]
reach=[f for f,m in man.items() if m["kind"]=="reach"]
uns=[(f,m) for f,m in probes if f not in fired and m["reach"] in fired]
print(len(probes), sum(1 for r in reach if r in fired), len(reach),
      sum(1 for _,m in uns if not m.get("impl_defined")),
      sum(1 for _,m in uns if m.get("impl_defined")))
PY
) || die "could not summarise arm $arm"
  set -- $a
  cases=$((cases+$1)); reaches_fired=$((reaches_fired+$2)); reaches_total=$((reaches_total+$3))
  unsound_airtight=$((unsound_airtight+$4)); unsound_impl=$((unsound_impl+$5))
done

# --- 3. verdict. Every clause is a POSITIVE requirement; none of them is "no error seen".
#
# CROSS-REPO CONTRACT. The "ITV GRID:" line below is consumed by pr/itv_check.sh, which
# lives in the HARNESS repository and is upgraded independently of this one. It reads
#     cases=<int>   reach_controls=<int>/<int>   calib=<int>/<int>
# and re-asserts them, so that a pass cannot be claimed by a run that tested nothing.
# ADDING fields here is safe. Renaming or dropping one of those three, or changing the
# <int>/<int> shape, breaks the consumer -- and it breaks it towards refusal, which blocks
# every push until both repositories agree again. The denominator is deliberately NOT
# fixed on either side: this file prints $CALIB_TOTAL and the consumer reads whatever
# denominator arrives and requires numerator == denominator, so adding a calibration case
# needs no coordinated change.
fail=0
[ "$cases" -gt 0 ] || { echo "OBLIGATION FAILED: the grid contributed 0 probes, so nothing was tested"; fail=1; }
[ "$reaches_fired" -gt 0 ] || { echo "OBLIGATION FAILED: no reach control fired, so every silent probe is inconclusive and this run proves nothing"; fail=1; }
[ "$controls" -eq "$CALIB_TOTAL" ] || { echo "OBLIGATION FAILED: calibration controls $controls/$CALIB_TOTAL"; fail=1; }

# A run must not modify the tree it is run from. This is asserted rather than assumed
# because the leak it caught -- clang writing calib.o beside the fixture it compiled --
# is INVISIBLE to `git status`: the repository ignores *.o. So the check stats the
# directory, and a git-based spelling of it would be vacuous here. See
# tree_clean_check.py.
stray=$(python3 -I "$HERE/tree_clean_check.py" "$HERE"); rc=$?
case $rc in
  0) ;;
  1) echo "OBLIGATION FAILED: the run left build artefacts in the source tree:"
     echo "$stray" | sed 's/^/    /'
     fail=1 ;;
  *) echo "OBLIGATION FAILED: could not check the source tree for artefacts"; fail=1 ;;
esac

# THE KNOWN SET IS PINNED BY IDENTITY, NOT BY COUNT. The grid currently finds unsound
# points that are real and already being worked (the band class, and the
# implementation-defined signed >> of a negative). A flat "must be zero" would make this
# check red from the day it lands; a tolerated COUNT would be worse, because 28 tolerates
# ANY 28 points -- a genuine new unsoundness could arrive while an old one is fixed and
# the total would not move. So expected/known_unsound.json names them, and the comparison
# is between SETS:
#   a point not in the pinned set            -> a new unsoundness, fail;
#   a pinned point that is now sound         -> ALSO fail, because that is a deliberate
#                                               semantic change and its baseline must be
#                                               updated in the same commit as the fix.
# For the sampled tier the pinned set is intersected with the probes actually generated,
# so a sample cannot be blamed for the points it never looked at.
set_cmp=$(python3 -I "$HERE/unsound_compare.py" "$HERE/expected/known_unsound.json" "$OUT" "$ARMS") ||
  die "could not compare against the pinned unsound set"
n_new=$(printf '%s' "$set_cmp" | python3 -I -c 'import json,sys; print(len(json.load(sys.stdin)["new"]))')
n_fixed=$(printf '%s' "$set_cmp" | python3 -I -c 'import json,sys; print(len(json.load(sys.stdin)["fixed"]))')
if [ "$n_new" -ne 0 ]; then
  echo "OBLIGATION FAILED: $n_new unsound point(s) NOT in verification/inferbo/expected/known_unsound.json:"
  printf '%s' "$set_cmp" | python3 -I -c 'import json,sys; [print("    "+f) for f in json.load(sys.stdin)["new"][:10]]'
  fail=1
fi
n_unknown=$(printf '%s' "$set_cmp" | python3 -I -c 'import json,sys; print(len(json.load(sys.stdin)["unknown"]))')
if [ "$n_unknown" -ne 0 ]; then
  echo "OBLIGATION FAILED: $n_unknown pinned point(s) name a probe that this run never generated. A pin that matches nothing is excused at every tier and counts as coverage that does not exist. Either it is a typo, or its quadruple is missing from verification/inferbo/expected/always_sample.json:"
  printf '%s' "$set_cmp" | python3 -I -c 'import json,sys; [print("    "+f) for f in json.load(sys.stdin)["unknown"][:10]]'
  fail=1
fi
if [ "$n_fixed" -ne 0 ]; then
  echo "OBLIGATION FAILED: $n_fixed pinned unsound point(s) are now SOUND. If that is the intended effect of this change, update verification/inferbo/expected/known_unsound.json in the SAME commit:"
  printf '%s' "$set_cmp" | python3 -I -c 'import json,sys; [print("    "+f) for f in json.load(sys.stdin)["fixed"][:10]]'
  fail=1
fi
[ -z "$manifest_drift" ] || { echo "OBLIGATION FAILED: regenerated manifest does not match expected/counts.json for:$manifest_drift -- gen.py's output moved; regenerate the baseline in the SAME commit as the change that moved it"; fail=1; }

echo "ITV GRID: tier=$TIER cases=$cases reach_controls=$reaches_fired/$reaches_total calib=$controls/$CALIB_TOTAL unsound_airtight=$unsound_airtight unsound_impl_defined=$unsound_impl seed=$SEED sample=${PERMILLE}permille"
if [ $fail -eq 0 ]; then
  echo "ALL CHECKS PASSED"
  exit 0
fi
echo "ITV GRID FAILED"
exit 1
