#!/bin/bash
# Driver for the Rocq half.  It needs a second opam switch and a Rocq install, so it is
# not part of Infer's build and `make` never reaches it.  The harness runs it, together
# with the bounded oracle, on any change to infer/src/pulse/PulseCItv.ml.
#
# A WORD ON WHAT A PASS MEANS HERE.  The proofs are about ONE reviewed revision of that
# module, pinned by sha256 in expected/source.sha256, so on a commit that edits it the
# drift step refuses and this driver exits non-zero.  That is the intended outcome, not
# a malfunction: regenerate with translate.sh, review the diff under v/, re-run the
# proofs and update the pin in the same commit.  See drift.sh for why the check is on
# the translation rather than on whether the proofs still compile.
#
#   ./run.sh            pins, proofs from clean, freshness attack, drift, mutants
#   ./run.sh --proofs   stop after the axiom check
set -u
cd "$(dirname "$0")" || exit 2
. ./setup.sh || exit 2

# PER-RUN SCRATCH, not a fixed /tmp path.  Two of these running at once -- a reviewer's copy
# while the standing check fires -- wrote the SAME files and silently corrupted each other's
# results, which is the worst shape a check can fail in: both runs look like they completed.
# CITV_KEEP_TMP=1 keeps the directory for debugging; failures already tail their own logs.
CITV_TMP=$(mktemp -d "${TMPDIR:-/tmp}/citv-run.XXXXXXXX") || exit 2
trap 'if [ "${CITV_KEEP_TMP:-0}" = 1 ]; then echo "citv: kept $CITV_TMP" >&2; else rm -rf "$CITV_TMP"; fi' EXIT

# Exit 2 throughout for "the check could not be carried out" -- a toolchain that does not
# match the pins, or a translator that does not resolve -- and 1 for "it ran and something
# failed".  check-axioms.sh already used 2 that way.  citv_check.sh distinguishes the two
# by the message rather than the code, but a human running this by hand should not have to.
echo "=== pins"
citv_assert_pins || { echo "REFUSED: toolchain does not match the pins"; exit 2; }
echo "ok: $CITV_PINS, translator $CITV_TRANSLATOR_COMMIT"

echo; echo "=== proofs, from clean"
./check-axioms.sh || exit 1
[ "${1:-}" = --proofs ] && exit 0

echo; echo "=== the freshness guard must refuse a clean that did not happen"
if CITV_CLEAN=false ./check-axioms.sh > "$CITV_TMP/freshattack.log" 2>&1; then
  echo "REFUSED: check-axioms.sh passed with a failing clean; the guard is not load-bearing"
  exit 1
fi
grep -q 'REFUSED' "$CITV_TMP/freshattack.log" \
  && echo "ok: refused, '$(grep -m1 REFUSED "$CITV_TMP/freshattack.log")'" \
  || { echo "REFUSED: it failed, but not via the freshness guard"; exit 1; }

echo; echo "=== the translator guard must refuse a directory, as a toolchain problem"
# A directory passes -x, so an earlier guard let CITV_TRANSLATOR=/some/dir through to
# translate.sh; the shell's failure to execute it then landed in the error list and came back
# out of drift as "the translator's error set changed" -- a missing toolchain reported as a
# soundness verdict about the commit.  Both halves are asserted, because only the pair
# distinguishes this from an ordinary failure: exit 2, AND a 'REFUSED: toolchain' line with
# no DRIFT line anywhere.  The directory used is this one, which certainly exists.
dir_out=$(CITV_TRANSLATOR="$PWD" ./drift.sh 2>&1); dir_rc=$?
if [ $dir_rc -eq 2 ] && printf '%s\n' "$dir_out" | grep -q '^REFUSED: toolchain' &&
   ! printf '%s\n' "$dir_out" | grep -q '^DRIFT:'; then
  echo "ok: refused, '$(printf '%s\n' "$dir_out" | grep -m1 '^REFUSED: toolchain' | cut -c1-90)'"
else
  echo "REFUSED: a directory as CITV_TRANSLATOR gave exit $dir_rc, not a toolchain refusal:"
  printf '%s\n' "$dir_out" | head -5
  exit 1
fi

echo; echo "=== drift"
./drift.sh; drift_rc=$?
[ $drift_rc -eq 0 ] || exit $drift_rc

echo; echo "=== mutants: each must be REJECTED"
./mutate.sh || exit 1

echo; echo "ALL CHECKS PASSED"
