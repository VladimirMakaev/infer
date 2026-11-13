#!/bin/bash
# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.
#
# Build the Lean development from clean and check what its theorems rest on. Usage:
#
#   ./check-axioms.sh [PROJECT_DIR]
#
# A green `lake build` is not enough on its own: a proof finished with `sorry` builds, and a
# proof that picked up a new assumption builds too. This reads the `#print axioms` reports in
# Citv/Axioms.lean back and refuses anything outside the whitelist.
#
# It builds from clean, and proves that it did. Lake REPLAYS the reports of a module it has not
# had to rebuild -- an incremental build prints all of them again, from the cache -- so seeing
# a report for every theorem says nothing about whether any theorem was checked just now. Three
# separate things establish freshness here: the clean has to succeed, the compiled artefact has
# to be gone afterwards, and the build has to say it BUILT the module rather than replayed it.
#
# CITV_CLEAN exists so that the first of those can be tested: `lean/run.sh` runs this script
# once with the clean forced to fail over a warm build, and requires it to refuse.

set -u -o pipefail

WHITELIST="propext Quot.sound Classical.choice"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT="${1:-$HERE}"

command -v lake >/dev/null || {
  echo "lake is not on PATH. Install the pinned toolchain with elan; see README.md." >&2
  exit 2
}

# A floor, for the same reason the counts below exist: an emptied Axioms.lean would otherwise
# report nothing and agree with itself about it.
MIN_THEOREMS=20
EXPECTED=$(grep -c '^#print axioms ' "$PROJECT/Citv/Axioms.lean")
if [ "$EXPECTED" -lt "$MIN_THEOREMS" ]; then
  echo "RESULT: FAIL -- Citv/Axioms.lean asks about $EXPECTED theorems and at least" >&2
  echo "$MIN_THEOREMS are expected." >&2
  exit 1
fi

CLEAN="${CITV_CLEAN:-lake clean}"
CLEAN_OUT="$( cd "$PROJECT" && $CLEAN 2>&1 )"
if [ $? -ne 0 ]; then
  printf '%s\n' "$CLEAN_OUT" | head -5
  echo "RESULT: FAIL -- '$CLEAN' failed, so the build that follows would not be from clean."
  exit 1
fi
STALE="$PROJECT/.lake/build/lib/lean/Citv/Axioms.olean"
if [ -e "$STALE" ]; then
  echo "RESULT: FAIL -- '$CLEAN' returned success but left $STALE behind."
  exit 1
fi

OUT="$( cd "$PROJECT" && lake build 2>&1 )"
STATUS=$?
if [ $STATUS -ne 0 ]; then
  printf '%s\n' "$OUT" | grep -E '^(error|warning)' | head -20
  echo "RESULT: FAIL -- lake build did not succeed."
  exit 1
fi
# Lake prints the same reports whether it checked the module or read it back from the cache,
# and says which it did. Only the first counts.
if printf '%s\n' "$OUT" | grep -q 'Replayed Citv.Axioms'; then
  echo "RESULT: FAIL -- lake replayed Citv.Axioms from its cache instead of checking it, so the"
  echo "reports below would be a recording rather than a result."
  exit 1
fi
if ! printf '%s\n' "$OUT" | grep -q 'Built Citv.Axioms'; then
  echo "RESULT: FAIL -- lake did not report building Citv.Axioms at all."
  exit 1
fi

echo "toolchain: $(cat "$PROJECT/lean-toolchain")"
echo "whitelist: $WHITELIST"
echo "freshness: $CLEAN succeeded, the artefact was gone, and lake reports Built, not Replayed"
echo

printf '%s\n' "$OUT" | sed -n "s/.*'\(Citv[^']*\)' depends on axioms: \[\(.*\)\]/\1\t\2/p" > /tmp/.citv-axioms.$$
printf '%s\n' "$OUT" | sed -n "s/.*'\(Citv[^']*\)' does not depend on any axioms.*/\1\t/p" >> /tmp/.citv-axioms.$$
REPORTS=$(grep -c . /tmp/.citv-axioms.$$)

BAD=0
while IFS=$'\t' read -r NAME AXIOMS; do
  [ -n "$NAME" ] || continue
  OFFENDERS=""
  for A in $(printf '%s' "$AXIOMS" | tr ',' ' '); do
    case " $WHITELIST " in *" $A "*) ;; *) OFFENDERS="$OFFENDERS $A" ;; esac
  done
  if [ -n "$OFFENDERS" ]; then
    echo "  REJECTED $NAME depends on:$OFFENDERS"
    BAD=$((BAD + 1))
  else
    echo "  ok       $NAME [${AXIOMS:-none}]"
  fi
done < /tmp/.citv-axioms.$$
rm -f /tmp/.citv-axioms.$$

echo
# A proof finished with sorry reports sorryAx above, and Lean also warns about it; take either.
if printf '%s\n' "$OUT" | grep -qi "declaration uses 'sorry'"; then
  echo "RESULT: FAIL -- the build warns that a declaration uses sorry."
  exit 1
fi
if [ "$REPORTS" -ne "$EXPECTED" ]; then
  echo "RESULT: FAIL -- $EXPECTED theorems are listed in Citv/Axioms.lean but only $REPORTS were"
  echo "reported, so some were never checked."
  exit 1
fi
if [ $BAD -ne 0 ]; then
  echo "RESULT: FAIL -- $BAD theorem(s) rest on something outside the whitelist."
  exit 1
fi
echo "RESULT: PASS ($REPORTS of $EXPECTED theorems checked, all inside the whitelist)"
