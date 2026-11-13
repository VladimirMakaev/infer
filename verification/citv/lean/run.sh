#!/bin/bash
# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.
#
# The unbounded half of the check on infer/src/pulse/PulseCItv.ml. See README.md.
#
#   ./verification/citv/lean/run.sh            prove, check the axioms, then break it on purpose
#   ./verification/citv/lean/run.sh --proofs    prove and check the axioms only
#
# Nothing here is part of Infer's build, and Infer's build does not depend on Lean: this
# directory is outside the dune root, and `make` and `dune build` never see it. The toolchain is
# pinned in lean-toolchain, the project has no dependencies, so the build is offline and needs
# no root.

set -u -o pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODE=all

# A loop over an empty file runs no iterations and reports no failures, which reads like a
# pass. A floor, not a count: adding a mutant needs no edit here, removing one does.
MIN_MUTANTS=6
while [ $# -gt 0 ]; do
  case "$1" in
    --proofs) MODE=proofs; shift ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done

command -v lake >/dev/null || {
  echo "lake is not on PATH. This needs the toolchain pinned in lean-toolchain:" >&2
  echo "  curl -sSf https://elan.lean-lang.org/elan-init.sh | sh   # user-local, no root" >&2
  echo "  elan toolchain install $(cat "$HERE/lean-toolchain")" >&2
  exit 2
}

echo "== the proofs, and what they rest on =="
"$HERE/check-axioms.sh" "$HERE"
STATUS=$?
if [ $STATUS -ne 0 ]; then
  echo
  echo "RESULT: FAIL -- the development does not check out."
  exit 1
fi

# The checker's own freshness guard, attacked. Lake REPLAYS the reports of a module it did not
# have to rebuild: an incremental build prints every one of them again, from the cache. So a
# check that merely counted them would pass over a stale tree, and the clean it does first has
# to be known to have worked rather than assumed to have. Force it to fail with the build warm
# and require a refusal.
echo
echo "== the checker's own freshness guard =="
( cd "$HERE" && lake build >/dev/null 2>&1 )
if CITV_CLEAN=false "$HERE/check-axioms.sh" "$HERE" >/dev/null 2>&1; then
  echo "  NOT REFUSED  a failed clean over a warm build"
  echo
  echo "RESULT: FAIL -- the axiom check would report on a cache it did not rebuild."
  exit 1
fi
echo "  refused      a failed clean over a warm build"

if [ $MODE = proofs ]; then
  echo
  echo "RESULT: PASS (proofs only; the mutants were not run)"
  exit 0
fi

# A development that cannot be broken is not saying anything. Each mutant must be refused,
# either by the proof checker or by the axiom whitelist, and it is recorded which.
echo
echo "== mutants: each must be refused =="
N_MUTANTS="$(grep -c "$(printf '\t')" "$HERE/mutants.txt")"
if [ "$N_MUTANTS" -lt "$MIN_MUTANTS" ]; then
  echo "RESULT: FAIL -- mutants.txt has $N_MUTANTS mutants and at least $MIN_MUTANTS are expected."
  exit 1
fi
SEEN=0
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
ACCEPTED=0
ACCEPTED_NAMES=""
while IFS=$'\t' read -r NAME EXPECTED OLD NEW; do
  case "$NAME" in ''|'#'*) continue ;; esac
  COPY="$WORK/try"
  rm -rf "$COPY"
  mkdir -p "$COPY/Citv"
  cp "$HERE/lean-toolchain" "$HERE/lakefile.toml" "$HERE/Citv.lean" "$COPY/"
  cp "$HERE/Citv/Comparison.lean" "$HERE/Citv/Axioms.lean" "$COPY/Citv/"
  if ! python3 - "$COPY/Citv/Comparison.lean" "$OLD" "$NEW" <<'PY'
import sys, pathlib
path, old, new = sys.argv[1], sys.argv[2].replace("\\n", "\n"), sys.argv[3].replace("\\n", "\n")
p = pathlib.Path(path)
s = p.read_text()
if s.count(old) != 1:
    sys.exit(f"edit anchor is not unique ({s.count(old)} matches)")
p.write_text(s.replace(old, new))
PY
  then
    echo "  ANCHOR LOST $NAME (the development has changed under it; the mutant needs updating)"
    ACCEPTED=$((ACCEPTED + 1))
    ACCEPTED_NAMES="$ACCEPTED_NAMES $NAME"
    continue
  fi
  OUT="$("$HERE/check-axioms.sh" "$COPY" 2>&1)"
  if [ $? -eq 0 ]; then
    OUTCOME=accepted
  elif printf '%s\n' "$OUT" | grep -q 'lake build did not succeed'; then
    OUTCOME=no-build
  else
    OUTCOME=axiom-reject
  fi
  SEEN=$((SEEN + 1))
  if [ "$OUTCOME" = "$EXPECTED" ]; then
    echo "  refused     $NAME: $OUTCOME"
  else
    echo "  NOT REFUSED $NAME: wanted $EXPECTED, got $OUTCOME"
    ACCEPTED=$((ACCEPTED + 1))
    ACCEPTED_NAMES="$ACCEPTED_NAMES $NAME"
  fi
done < "$HERE/mutants.txt"

echo
if [ $ACCEPTED -ne 0 ]; then
  echo "RESULT: FAIL -- these were not refused:$ACCEPTED_NAMES"
  exit 1
fi
if [ "$SEEN" -ne "$N_MUTANTS" ]; then
  echo "RESULT: FAIL -- $N_MUTANTS mutants are in the file but $SEEN were run."
  exit 1
fi
echo "RESULT: PASS ($SEEN of $N_MUTANTS refused)"
