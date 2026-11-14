#!/bin/bash
# Three INDEPENDENT drift checks, each with its own message.
#
# The third is the one that must not be dropped.  A passing proof after a source change
# proves nothing about the change: if the edited function is not named by any theorem,
# every proof still compiles and the run says exactly what it said before.  So the check
# is on the TRANSLATION, not on the proof outcome.
set -u
cd "$(dirname "$0")" || exit 2
. ./setup.sh || exit 2

ROOT=$(git rev-parse --show-toplevel 2>/dev/null) || { echo "not in a git tree"; exit 2; }
SRC=infer/src/pulse/PulseCItv.ml
FAIL=0
bad() { printf 'DRIFT: %s\n' "$*"; FAIL=1; }

# ------------------------------------------------- 1. has the source moved?
want=$(awk '{print $1}' expected/source.sha256)
got=$(sha256sum "$ROOT/$SRC" | awk '{print $1}')
if [ "$want" != "$got" ]; then
  bad "$SRC changed."
  echo "      expected $want"
  echo "      found    $got"
  echo "      -> regenerate (translate.sh), review the diff of v/, re-run the proofs."
else
  echo "ok 1/3  $SRC matches the reviewed revision"
fi

# ------------------------------------------- 2. has the translator's surface moved?
# A NEW unsupported construct, or one that silently disappears, both matter: the second
# would mean the generated file now means something different at that site.
if [ -z "${CITV_TRANSLATOR:-}" ]; then
  echo "skip 2/3  CITV_TRANSLATOR not set"
else
  ./translate.sh --errors-only > /tmp/citv-errs.txt 2>&1 || true
  if ! diff -u expected/translator-errors.txt /tmp/citv-errs.txt > /tmp/citv-errdiff.txt; then
    bad "the translator's error set changed"; cat /tmp/citv-errdiff.txt
  else
    echo "ok 2/3  translator reports exactly the $(wc -l < expected/translator-errors.txt) known sites"
  fi
fi

# --------------------------------------- 3. does the translation still regenerate?
if [ -z "${CITV_TRANSLATOR:-}" ]; then
  if [ "${CITV_ALLOW_NO_TRANSLATOR:-0}" = 1 ]; then
    echo "skip 3/3  CITV_TRANSLATOR not set and CITV_ALLOW_NO_TRANSLATOR=1"
    echo "          NOTE: checks 2 and 3 did not run.  A pass here is weaker than a full run."
  else
    bad "CITV_TRANSLATOR is not set, so the translation could not be regenerated."
    echo "      Point it at rocqOfOCaml.exe built from $CITV_TRANSLATOR_COMMIT, or set"
    echo "      CITV_ALLOW_NO_TRANSLATOR=1 to accept a weaker run deliberately."
  fi
else
  rm -rf /tmp/citv-regen && mkdir -p /tmp/citv-regen
  if ./translate.sh --out /tmp/citv-regen > /tmp/citv-regen.log 2>&1; then
    for f in IStd IntLit Binop Unop PulseCItv; do
      if ! cmp -s "v/$f.v" "/tmp/citv-regen/$f.v"; then
        bad "regenerated $f.v differs from the committed one"
        diff -u "v/$f.v" "/tmp/citv-regen/$f.v" | head -40
      fi
    done
    [ $FAIL -eq 0 ] && echo "ok 3/3  all five generated files regenerate byte-identically"
  else
    bad "translate.sh failed"; tail -20 /tmp/citv-regen.log
  fi
fi

[ $FAIL -eq 0 ] && { echo "drift: clean"; exit 0; }
echo "drift: REFUSED"
exit 1
