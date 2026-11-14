#!/bin/bash
# refresh-pins.sh : regenerate the two files in expected/ that drift.sh compares against.
#
#   expected/source.sha256        the reviewed revision of infer/src/pulse/PulseCItv.ml
#   expected/translator-errors.txt the translator's error sites for the current source
#
# Run this after editing PulseCItv.ml AND reviewing the regenerated v/ diff -- not instead
# of reviewing it. Refreshing the pins is what tells drift.sh "a human has looked at this
# version"; doing it without looking is the one way to make the whole Rocq half meaningless.
#
# WHY A SCRIPT RATHER THAN A REDIRECT. The obvious way to do this is
#
#     ./translate.sh --errors-only > expected/translator-errors.txt
#
# and it is a trap: the shell truncates the pin BEFORE running the command, so a run that
# fails for any reason -- a missing opam root, say -- leaves an EMPTY pin behind. An empty
# pin then makes drift check 2 compare nothing against nothing and pass. Measured on
# 2026-11-14: exactly that happened, and the pin had to be restored from git. So this
# writes to a temporary file, checks the status AND that the result is non-empty, and only
# then moves it into place.
set -u
cd "$(dirname "$0")" || exit 2
. ./setup.sh || exit 2

ROOT=$(git rev-parse --show-toplevel) || { echo "not in a git tree"; exit 2; }
SRC=infer/src/pulse/PulseCItv.ml
TMP=$(mktemp -d) || exit 2
trap 'rm -rf "$TMP"' EXIT

# --- the source pin
sha256sum "$ROOT/$SRC" | awk -v p="$SRC" '{print $1 "  " p}' > "$TMP/source.sha256" || exit 2
[ -s "$TMP/source.sha256" ] || { echo "refusing: the source pin came out empty"; exit 1; }

# --- the translator error pin
if [ -z "${CITV_TRANSLATOR:-}" ]; then
  echo "CITV_TRANSLATOR is not set, so the error pin cannot be regenerated."
  echo "Point it at rocqOfOCaml.exe built from $CITV_TRANSLATOR_COMMIT."
  exit 2
fi
if ! ./translate.sh --errors-only > "$TMP/translator-errors.txt" 2>"$TMP/err.log"; then
  echo "refusing: translate.sh failed, so the old pin is kept"
  cat "$TMP/translator-errors.txt" "$TMP/err.log" 2>/dev/null | head -20
  exit 1
fi
# An empty error set is not impossible in principle, but it is overwhelmingly more likely
# to mean the build silently produced no .cmt than that the translator suddenly renders
# every construct. Refuse and make the human say so.
if [ ! -s "$TMP/translator-errors.txt" ]; then
  echo "refusing: the regenerated error set is EMPTY, which almost always means the stub"
  echo "build produced no .cmt rather than that the translator now handles everything."
  echo "If it is genuinely empty, write the file by hand and say why in the commit."
  exit 1
fi

for f in source.sha256 translator-errors.txt; do
  if cmp -s "$TMP/$f" "expected/$f"; then
    echo "unchanged  expected/$f"
  else
    cp "$TMP/$f" "expected/$f"
    echo "updated    expected/$f"
  fi
done
echo
echo "Now review the regenerated translation (./translate.sh --out DIR and diff against v/)"
echo "and re-run ./run.sh before committing."
