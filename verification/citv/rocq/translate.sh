#!/bin/bash
# Regenerate the Rocq translation from the REAL source, reproducibly.
#
#   ./translate.sh --out DIR     write the five generated .v files to DIR
#   ./translate.sh --errors-only print the translator's error sites, normalised
#
# The source is read from the worktree and never copied into this directory, so this
# directory cannot hold a stale fork of PulseCItv.ml.
#
# CITV_TRANSLATOR must point at rocqOfOCaml.exe built from the pinned commit (see
# setup.sh).  It is not vendored here: the public tree carries the generated .v and the
# proofs, which stand alone, and the translator tarball lives in the private harness.
#
# WHY THE STUBS ARE NOT verification/citv/shims.  The design said to drive this from M1's
# shipped shims.  That works for PulseCItv.v itself - the two translations differ in 5
# cosmetic lines and no function body - but NOT for the dependencies: M1's IntLit is
# Zarith-backed, and Zarith is opaque to the translator (it has no .cmt for it), so the
# M1-driven IntLit.v is nothing but op_startypeminuserrorstar error markers and is
# unusable.  stubs/ is therefore the int-backed set, which the translator maps to Rocq's
# unbounded Z - the same domain as the real Z.t-backed IntLit.
set -u
cd "$(dirname "$0")" || exit 2
. ./setup.sh || exit 2

MODE=${1:---out}
OUT=${2:-}
[ "$MODE" = --errors-only ] || [ -n "$OUT" ] || { echo "usage: $0 --out DIR | --errors-only"; exit 2; }

ROOT=$(git rev-parse --show-toplevel) || exit 2
[ -n "${CITV_TRANSLATOR:-}" ] || { echo "CITV_TRANSLATOR is not set"; exit 2; }
[ -x "$CITV_TRANSLATOR" ] || { echo "CITV_TRANSLATOR is not executable: $CITV_TRANSLATOR"; exit 2; }

BUILD=$(mktemp -d)
trap 'rm -rf "$BUILD"' EXIT
mkdir -p "$BUILD/target"
cp stubs/*.ml stubs/dune stubs/dune-project "$BUILD/target/"
# the module under test, straight from the worktree.  The capitalised file name matters:
# it is what the translator resolves the .cmt against.
cp "$ROOT/infer/src/pulse/PulseCItv.ml" "$BUILD/target/PulseCItv.ml"

# the .cmt is the translator's input, so the file has to typecheck first
( cd "$BUILD/target" && $ROCQ_RUN dune build @check ) > "$BUILD/dune.log" 2>&1 || {
  echo "the OCaml build failed; the translator needs a .cmt"; tail -20 "$BUILD/dune.log"; exit 1; }

# The translator looks for the .cmt/.cmi BESIDE the source, not under _build, and it
# resolves the dependencies' types the same way.  Without this the output still "works"
# but silently degrades: IntLit.t comes out as a type variable and the module aliases
# collapse to empty modules.  So this copy is load-bearing, not tidying.
find "$BUILD/target/_build" \( -name '*.cmt' -o -name '*.cmi' \) -exec cp {} "$BUILD/target/" \;

RAW="$BUILD/raw"; mkdir -p "$RAW"
ERRS="$BUILD/errors.txt"; : > "$ERRS"
( cd "$BUILD" || exit 2
  for m in iStd intLit binop unop logging PulseCItv; do
    V=$(python3 -I -c 'import sys;n=sys.argv[1];print(n[0].upper()+n[1:])' "$m")
    "$CITV_TRANSLATOR" -output "$RAW/$V.v" "target/$m.ml" > "$BUILD/$m.out" 2>&1
    # The translator's ERROR headers are the only '---' lines carrying a category and an
    # (n/m) counter; plain source-context lines do not.  Reduce each to "file:line category".
    sed -e 's/\x1b\[[0-9;]*m//g' "$BUILD/$m.out" \
      | grep -oE '[^ /]+\.ml:[0-9]+:[0-9]+ -+ [a-z_]+ \([0-9]+/[0-9]+\)' \
      | sed -E 's/:[0-9]+ -+ / /; s/ \([0-9]+\/[0-9]+\)$//' >> "$ERRS"
  done )

if [ "$MODE" = --errors-only ]; then
  # 'merlin' headers are informational (one per typed node); the categories that mean the
  # translator could not render a construct are not_supported and side_effect.
  grep -E ' (not_supported|side_effect)$' "$ERRS" | sort
  exit 0
fi

python3 -I strip.py "$RAW" "$OUT" > /dev/null || exit 1
python3 -I repair.py "$OUT" > /dev/null || exit 1
exit 0
