#!/bin/bash
# Opt-in driver.  Not a standing check: it needs a second opam switch and a Rocq install.
#
#   ./run.sh            pins, proofs from clean, freshness attack, drift, mutants
#   ./run.sh --proofs   stop after the axiom check
set -u
cd "$(dirname "$0")" || exit 2
. ./setup.sh || exit 2

echo "=== pins"
citv_assert_pins || { echo "REFUSED: toolchain does not match the pins"; exit 1; }
echo "ok: $CITV_PINS, translator $CITV_TRANSLATOR_COMMIT"

echo; echo "=== proofs, from clean"
./check-axioms.sh || exit 1
[ "${1:-}" = --proofs ] && exit 0

echo; echo "=== the freshness guard must refuse a clean that did not happen"
if CITV_CLEAN=false ./check-axioms.sh > /tmp/citv-freshattack.log 2>&1; then
  echo "REFUSED: check-axioms.sh passed with a failing clean; the guard is not load-bearing"
  exit 1
fi
grep -q 'REFUSED' /tmp/citv-freshattack.log \
  && echo "ok: refused, '$(grep -m1 REFUSED /tmp/citv-freshattack.log)'" \
  || { echo "REFUSED: it failed, but not via the freshness guard"; exit 1; }

echo; echo "=== drift"
./drift.sh || exit 1

echo; echo "=== mutants: each must be REJECTED"
./mutate.sh || exit 1

echo; echo "ALL CHECKS PASSED"
