#!/bin/bash
# run.sh [--tier pre-push|nightly] [--outdir DIR] : property and differential checks on the
# Pulse formula engine. Content-side runner, called by the harness's pr/pulse_formula_check.sh.
# Runnable by hand with the same arguments.
#
#   Exit 0  every obligation held. Prints "ALL CHECKS PASSED".
#   Exit 1  an obligation failed (a violation count moved, or a property did not reach its
#           condition).
#   Exit 2  the checks could not be run. Nothing is claimed.
#
# THE ENGINE SOURCES ARE TAKEN LIVE, NOT COMMITTED HERE. The properties are about
# infer/src/pulse/PulseFormula*.ml, and the only way to test those with a few hundred lines
# of shims instead of the whole analyser is to compile them outside their normal build. The
# obvious way to do that is to keep reviewed copies beside the shims -- and that is a trap:
# a copy that is not refreshed still compiles and still passes, so the check would report a
# clean run about code that is no longer shipped. So nothing is copied in advance. The
# runner assembles a build directory from the shims plus the engine files AS THEY STAND IN
# THIS TREE, and prints their hashes in the trailer as evidence of what was actually tested.
# If a change to one of them outgrows the shims the build fails, which is exit 2 and a
# request to update the shims -- never a silent pass.
#
# WHAT IS ASSERTED, POSITIVELY. Violation counts are matched EXACTLY and in both
# directions, and reach counters have floors. A property that stops firing is as much a
# change as one that starts, and a clean result from a property that never reached its
# condition is the failure this family of harnesses keeps producing: the normtest residuals
# below are real and triaged, so they are pinned rather than tolerated.
set -u
export LC_ALL=C

me=pulse-formula
die() { echo "$me: $*" >&2; echo "PF CHECKS INCOMPLETE: $*"; exit 2; }

HERE=$(cd "$(dirname "$(readlink -f "$0")")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)           # the infer checkout this runner lives in
PULSE=$ROOT/infer/src/pulse
TIER=pre-push; OUT=""
: "${PF_SEED:=20261116}"
: "${CITV_HOME:=$HOME/.citv-rocq}"
: "${PF_SWITCH:=fv}"

while [ $# -gt 0 ]; do
  case $1 in
    --tier)   [ $# -ge 2 ] || die "--tier needs a value"; TIER=$2; shift 2 ;;
    --outdir) [ $# -ge 2 ] || die "--outdir needs a path"; OUT=$2; shift 2 ;;
    *) die "usage: $me [--tier pre-push|nightly] [--outdir DIR]" ;;
  esac
done
case $TIER in pre-push|nightly) ;; *) die "--tier must be 'pre-push' or 'nightly', not '$TIER'" ;; esac

# The engine files the shims are built against. A file that disappears or is renamed is a
# hard stop: the harness would otherwise quietly test a smaller surface than it claims.
ENGINE="PulseFormulaTerm.ml PulseFormulaLinArit.ml PulseFormulaLinArit.mli
PulseFormulaAtom.ml PulseFormulaTableau.ml PulseFormulaDebug.ml
QSafeCapped.ml QSafeCapped.mli ZSafe.ml"
for f in $ENGINE; do
  [ -f "$PULSE/$f" ] ||
    die "$PULSE/$f does not exist; the property harness names an engine file that is no longer there, so update verification/pulse-formula in the same commit as the rename"
done

# SHIM COVERAGE, before the build and before the toolchain is even needed. The build already
# refuses when a shim has fallen behind, but the compiler stops at the first unbound name, so
# the operator fixes one symbol, re-runs, and meets the next. On 2026-11-24 the reported failure
# was Config.pulse_opaque_max_size and behind it sat Config.pulse_opaque_max_depth and
# Var.mk_fresh; two of the three were invisible until the first was fixed. This names the whole
# set in one pass. It is a coverage check, not a type check -- the build still settles types,
# and only a human settles values.
[ -x "$HERE/shim-coverage.sh" ] ||
  die "shim-coverage.sh is missing or not executable beside $HERE; it is part of this harness"
"$HERE/shim-coverage.sh" "$PULSE" "$HERE/shims" $ENGINE ||
  die "the shims have fallen behind the engine files in this tree (see the list above); update them in the same commit as the change that outgrew them"

[ -d "$CITV_HOME/bin" ] && PATH="$CITV_HOME/bin:$PATH"
export PATH
command -v opam >/dev/null 2>&1 ||
  die "opam is not on PATH and none is at $CITV_HOME/bin/opam; build the toolchain with verification/citv/install-toolchain.sh (override the location with CITV_HOME)"
export OPAMROOT="${CITV_OPAMROOT:-$CITV_HOME/opamroot}"
[ -d "$OPAMROOT" ] || die "no opam root at $OPAMROOT; build it with verification/citv/install-toolchain.sh"
opam switch list --short 2>/dev/null | grep -qxF "$PF_SWITCH" ||
  die "switch '$PF_SWITCH' is not in $OPAMROOT (override with PF_SWITCH)"
[ -f "$HERE/expected/counts.json" ] || die "no expected/counts.json beside $HERE; there is no baseline to judge against"

if [ -z "$OUT" ]; then
  OUT=$(mktemp -d -t pulse-formula-XXXXXX) || die "could not create a work directory"
  trap 'rm -rf "$OUT"' EXIT
fi
mkdir -p "$OUT/src" || die "could not create $OUT/src"

# --- 1. assemble the build directory: shims + tests + the LIVE engine files
cp "$HERE"/shims/*.ml "$OUT/src/" || die "could not copy the shims"
cp "$HERE"/tests/*.ml "$OUT/src/" || die "could not copy the property tests"
engine_hashes=""
for f in $ENGINE; do
  cp "$PULSE/$f" "$OUT/src/$f" || die "could not copy $f out of the tree"
  engine_hashes="$engine_hashes $f:$(sha256sum "$PULSE/$f" | cut -c1-12)"
done

# The gmp the switch was built against lives under CITV_HOME, whose path is a property of
# the host rather than of this repository, so the dune file is generated rather than
# committed with somebody's home directory baked into it.
echo '(lang dune 3.0)' > "$OUT/dune-project"
cat > "$OUT/src/dune" <<EOF
(executables
 (names test termtest normtest termprobe probe)
 (libraries zarith yojson)
 (preprocess (pps ppx_compare ppx_yojson_conv))
 (link_flags (-cclib -L$CITV_HOME/prefix/lib -cclib -lgmp))
 (flags (:standard -w -a)))
EOF

if [ -d "$CITV_HOME/prefix/lib" ]; then
  LD_LIBRARY_PATH="$CITV_HOME/prefix/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
  export LD_LIBRARY_PATH
fi

echo "== $me: tier=$TIER seed=$PF_SEED"
echo "== $me: engine files under test:$engine_hashes"

build_log=$OUT/build.log
( cd "$OUT" && opam exec --switch="$PF_SWITCH" -- dune build ./src/test.exe ./src/termtest.exe \
    ./src/normtest.exe ./src/termprobe.exe ) >"$build_log" 2>&1 ||
  die "the property harness did not build against the engine files in this tree; the shims need updating in the same commit as the change that outgrew them (see $build_log): $(head -3 "$build_log" | tr '\n' ' ')"

# --- 2. run the properties
if [ "$TIER" = nightly ]; then LIN_N=200000; TERM_N=400000; NORM_N=300000
else LIN_N=20000; TERM_N=40000; NORM_N=30000; fi

run_one () { # name exe args...
  local name=$1; shift
  local exe=$1; shift
  ( cd "$OUT" && opam exec --switch="$PF_SWITCH" -- "./_build/default/src/$exe.exe" "$@" ) \
    >"$OUT/$name.out" 2>&1
  local rc=$?
  # A runner that died has not produced a verdict, and its missing violations must never
  # read as zero violations.
  if [ $rc -ne 0 ]; then
    die "$name exited $rc without completing: $(tail -3 "$OUT/$name.out" | tr '\n' ' ')"
  fi
}
run_one linarith  test     "$PF_SEED" "$LIN_N" big
run_one term      termtest "$PF_SEED" "$TERM_N"
run_one norm      normtest "$PF_SEED" "$NORM_N"

# --- 3. judge against the committed baseline
summary=$(python3 "$HERE/summarise.py" "$HERE/expected/counts.json" "$TIER" \
            linarith="$OUT/linarith.out" term="$OUT/term.out" norm="$OUT/norm.out" 2>"$OUT/judge.err")
judge_rc=$?
if [ $judge_rc -eq 2 ]; then
  cat "$OUT/judge.err" >&2
  die "the property output could not be judged"
fi
[ -s "$OUT/judge.err" ] && cat "$OUT/judge.err"

forms=$(printf '%s' "$summary" | python3 -c 'import json,sys; print(json.load(sys.stdin)["linarith"]["reach"].get("iterations",0))')
abst=$(printf '%s' "$summary" | python3 -c 'import json,sys; print(json.load(sys.stdin)["linarith"]["reach"].get("abstained",0))')
termreach=$(printf '%s' "$summary" | python3 -c 'import json,sys
r=json.load(sys.stdin)["term"]["reach"]; print(r.get("bitwise operand outside int64",0))')
e1=$(printf '%s' "$summary" | python3 -c 'import json,sys
r=json.load(sys.stdin)["norm"]["reach"]; print(r.get("E1 both sides folded to constants",0))')

echo "PF GRID: tier=$TIER forms=$forms abstained=$abst cap_reach=$termreach e1_reach=$e1 seed=$PF_SEED"
if [ $judge_rc -eq 0 ]; then
  echo "ALL CHECKS PASSED"
  exit 0
fi
echo "PF CHECKS FAILED"
exit 1
