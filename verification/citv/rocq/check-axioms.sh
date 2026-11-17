#!/bin/bash
# What the proofs rest on, and whether they were checked just now.
#
# This is the acceptance criterion on its own.  It is deliberately STRICTER than the Lean
# check one directory up: there, three axioms are whitelisted because the Lean kernel is
# normally trusted with them.  Here NOTHING is whitelisted, because the support library
# every generated file imports declares
#     Parameter assert : forall (A : Set), bool -> A
# which is flatly inconsistent - Axioms.v proves False from it in two lines.  There is no
# sound subset of this library's parameters to allow, so the only acceptable report is
# "Closed under the global context".
#
# Note what this does NOT cover, so that nobody reads more into a pass than is there:
# rocq-of-ocaml emits the OCaml asserts in two shapes, and only one reaches the kernel.
#   - statement position (PulseCItv.ml:129 in between, :134 in outside) becomes
#     "let '_ := assert unit (...) in ...", which Rocq's elaborator DISCARDS: the compiled
#     between is literally "fun b1 b2 => Between b1 b2".  No axiom, nothing to detect.
#   - value position (PulseCItv.ml:568 in requires_integer_reasoning) survives, and any
#     theorem touching it reports Basics.assert and is refused here.
# So this gate is an assert gate for :568 only.  See README, "What is not covered".
set -u
cd "$(dirname "$0")" || exit 2

# PER-RUN SCRATCH, not a fixed /tmp path.  Two of these running at once -- a reviewer's copy
# while the standing check fires -- wrote the SAME files and silently corrupted each other's
# results, which is the worst shape a check can fail in: both runs look like they completed.
# CITV_KEEP_TMP=1 keeps the directory for debugging; failures already tail their own logs.
CITV_TMP=$(mktemp -d "${TMPDIR:-/tmp}/citv-axioms.XXXXXXXX") || exit 2
trap 'if [ "${CITV_KEEP_TMP:-0}" = 1 ]; then echo "citv: kept $CITV_TMP" >&2; else rm -rf "$CITV_TMP"; fi' EXIT

FAIL=0
say() { printf '%s\n' "$*"; }
bad() { printf 'REFUSED: %s\n' "$*"; FAIL=1; }

[ -n "${ROCQ_RUN:-}" ] || { say "ROCQ_RUN is not set; run through run.sh (it sources setup.sh)"; exit 2; }

cd v || exit 2

# ---------------------------------------------------------------- freshness, positively
# A stale .vo is Rocq's version of Lake replaying a cached report: every theorem would be
# reported again without anything being checked.  So the artefacts have to be GONE, and
# that is verified rather than assumed.
CLEAN=${CITV_CLEAN:-rm}
if [ "$CLEAN" = rm ]; then
  rm -f ./*.vo ./*.vok ./*.vos ./*.glob ./.*.aux
  rc=$?
else
  $CLEAN                       # CITV_CLEAN=false is how run.sh attacks this guard
  rc=$?
fi
[ $rc -eq 0 ] || bad "the clean step exited $rc"
leftover=$(ls ./*.vo 2>/dev/null | wc -l)
[ "$leftover" -eq 0 ] || bad "$leftover .vo files survived the clean, so a pass would prove nothing"
[ $FAIL -eq 0 ] || { say "FAILED (freshness)"; exit 1; }

# ---------------------------------------------------------------------------- build
for f in IStd IntLit Binop Unop PulseCItv Soundness; do
  $ROCQ_RUN rocq compile -Q . "" $f.v > "$CITV_TMP/rocq-$f.log" 2>&1 || {
    bad "$f.v did not compile"; tail -20 "$CITV_TMP/rocq-$f.log"; say "FAILED (build)"; exit 1; }
done
[ -f Soundness.vo ] || { bad "Soundness.vo was not produced"; say "FAILED (build)"; exit 1; }

# ------------------------------------------------------- the theorem set, by set equality
# A count would accept any prefix.  Both directions are checked: a theorem that is deleted
# or renamed fails, and a theorem that is added without being declared here fails too, so
# the expected list cannot quietly fall behind the file.
EXPECTED=../expected/theorems.txt
grep -oE '^(Theorem|Example) [A-Za-z_0-9]+' Soundness.v | awk '{print $2}' | sort -u > "$CITV_TMP/actual.txt"
sort -u "$EXPECTED" > "$CITV_TMP/expected.txt"
if ! diff -u "$CITV_TMP/expected.txt" "$CITV_TMP/actual.txt" > "$CITV_TMP/setdiff.txt"; then
  bad "the theorem set does not match expected/theorems.txt"
  cat "$CITV_TMP/setdiff.txt"
fi
N=$(wc -l < "$CITV_TMP/expected.txt")
[ "$N" -ge 38 ] || bad "expected/theorems.txt lists only $N names; this milestone has 38"

# ---------------------------------------------- the README must not name a dead theorem
# README.md's "What is proved" table listed abduce_le_sat_is_unsound_on_outside_lhs long
# after that theorem had been replaced, and said the Outside/Between arm was still
# excluded by a hypothesis after the arm had been repaired.  Nothing noticed, because
# nothing compared the table with the file.  ONE DIRECTION ONLY: every theorem-looking
# name in the table must exist, but the table is allowed not to list everything, because
# some of its rows group several results behind a wildcard ("four witness_* results").
README=../README.md
if [ -f "$README" ]; then
  grep -oE '^(Theorem|Example|Lemma|Definition) [A-Za-z_0-9]+' Soundness.v | awk '{print $2}' \
    | sort -u > "$CITV_TMP/defined.txt"
  # Two tables, and the Rocq name is in a different column in each: the results table
  # names it first, the Lean-to-Rocq mapping names it second.  Only that column is read.
  # The surrounding prose is not: a backtick in prose is a word, not a claim that a
  # theorem exists.  Rows that group several results behind a wildcard (`witness_*`) fall
  # out by the same rule, since the wildcard is not an identifier.
  readme_table () { # readme_table <heading> <column>
    awk -v h="$1" '$0 == h {inside=1; next} /^#{2,4} /{inside=0} inside && /^\| /' "$README" \
      | cut -d'|' -f"$2" | grep -oE '`[A-Za-z_][A-Za-z_0-9]*`' | tr -d '`' | sort -u
  }
  { readme_table '## What is proved' 2
    readme_table '#### The mapping, Lean theorem to Rocq theorem' 3
  } | sort -u > "$CITV_TMP/named.txt"
  [ -s "$CITV_TMP/named.txt" ] || bad "no theorem names found in README.md's tables; the check is not reading them"
  ghosts=$(comm -23 "$CITV_TMP/named.txt" "$CITV_TMP/defined.txt")
  if [ -n "$ghosts" ]; then
    bad "README.md's tables name results that Soundness.v does not define:"
    sed 's/^/      /' <<<"$ghosts"
  fi
fi

# ------------------------------------------------------------------- Print Assumptions
# Produced by a FRESHLY compiled file, so the reports cannot come from a cache.
{ echo 'Require Import Soundness.'
  while read -r t; do [ -n "$t" ] && echo "Print Assumptions Soundness.$t."; done < "$CITV_TMP/expected.txt"
} > CheckAxioms.v
$ROCQ_RUN rocq compile -Q . "" CheckAxioms.v > "$CITV_TMP/axioms.log" 2>&1
rc=$?
rm -f CheckAxioms.v CheckAxioms.vo CheckAxioms.glob CheckAxioms.vok CheckAxioms.vos .CheckAxioms.aux
[ $rc -eq 0 ] || { bad "Print Assumptions run failed (a named theorem may not exist)"
                   tail -20 "$CITV_TMP/axioms.log"; say "FAILED (axioms)"; exit 1; }

REPORTS=$(grep -c '^Closed under the global context' "$CITV_TMP/axioms.log")
[ "$REPORTS" -eq "$N" ] || bad "$REPORTS clean reports for $N theorems"
if grep -qE '^Axioms:|^Axiom |sorry|Admitted' "$CITV_TMP/axioms.log"; then
  bad "an axiom or an admitted proof was reported"
  grep -nE -A2 '^Axioms:' "$CITV_TMP/axioms.log" | head -20
fi
# and no theorem may be Admitted in the source either
if grep -qE '\bAdmitted\b|\bsorry\b' Soundness.v; then bad "Soundness.v contains Admitted"; fi

if [ $FAIL -eq 0 ]; then
  say "PASS ${N}/${N} theorems, every one Closed under the global context, built from clean"
  exit 0
fi
say "FAILED"
exit 1
