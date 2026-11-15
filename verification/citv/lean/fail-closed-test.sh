#!/bin/bash
# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.
#
# Every refusal in check-axioms.sh must actually refuse.
#
# WHY THIS EXISTS.  Three of its tests asked "is this bad string in the build output?" as
# `printf '%s\n' "$OUT" | grep -q ...` under `set -o pipefail`.  grep -q exits at the first
# match, SIGPIPE kills the writer, and pipefail turns the pipeline's status into 141 -- so a
# match reported FAILURE and the bad string was treated as absent.  Two of the three sites
# were therefore fail-open: a replayed cache, or a proof finished with `sorry`, would have been
# waved through.  Nothing caught it because it only bites once the output exceeds the 64KB pipe
# buffer, and every hand test had a short output.
#
# HOW IT TESTS.  It runs the REAL check-axioms.sh against a stub `lake` that prints a chosen
# build output, so each refusal is exercised through the production code path rather than
# through a copy of the logic.  The stub also means no Lean toolchain is needed and the whole
# thing takes a second.  Every scenario is run with the output padded PAST the pipe buffer,
# because a short one passes even with the bug.
#
#   ./fail-closed-test.sh
set -u -o pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CHECKER="${CITV_CHECKER:-$HERE/check-axioms.sh}"
N_THEOREMS=20            # must be >= the checker's own MIN_THEOREMS floor
PAD_BYTES=200000         # comfortably past the 64KB pipe buffer

WORK="$(mktemp -d "${TMPDIR:-/tmp}/citv-failclosed.XXXXXXXX")" || exit 2
trap 'rm -rf "$WORK"' EXIT

# A project the checker is willing to look at: the toolchain file, and an Axioms.lean with
# enough `#print axioms` lines to clear the floor.
PROJ="$WORK/proj"
mkdir -p "$PROJ/Citv" "$PROJ/.lake/build/lib/lean/Citv"
cp "$HERE/lean-toolchain" "$PROJ/lean-toolchain"
: > "$PROJ/Citv/Axioms.lean"
for i in $(seq 1 $N_THEOREMS); do echo "#print axioms Citv.t$i" >> "$PROJ/Citv/Axioms.lean"; done

# The stub. `lake clean` removes the artefact the checker insists must be gone; `lake build`
# prints the scenario named in CITV_STUB_CASE.  The reports are what a clean run looks like.
mkdir -p "$WORK/bin"
cat > "$WORK/bin/lake" <<'STUB'
#!/bin/bash
PROJ=$(pwd)
case "${1:-}" in
  clean) rm -f "$PROJ/.lake/build/lib/lean/Citv/Axioms.olean"; exit 0 ;;
esac
case "$CITV_STUB_CASE" in
  replayed) echo "Replayed Citv.Axioms" ;;
  notbuilt) : ;;                      # no "Built Citv.Axioms" line at all
  *)        echo "Built Citv.Axioms" ;;
esac
[ "$CITV_STUB_CASE" = sorry ] && echo "warning: declaration uses 'sorry'"
for i in $(seq 1 "$CITV_STUB_N"); do echo "info: 'Citv.t$i' depends on axioms: [propext]"; done
# pad past the pipe buffer, so a SIGPIPE-sensitive test is actually exposed
python3 -I -c "import sys; sys.stdout.write('pad line\n' * ($CITV_STUB_PAD // 9))"
exit 0
STUB
chmod +x "$WORK/bin/lake"

fails=0
# case NAME EXPECT(refuse|pass) WANTED-TEXT
case_is() {
  local name=$1 expect=$2 want=${3:-}
  : > "$PROJ/.lake/build/lib/lean/Citv/Axioms.olean"      # something for `clean` to remove
  local out rc
  out=$(cd "$PROJ" && PATH="$WORK/bin:$PATH" CITV_STUB_CASE="$name" \
        CITV_STUB_N="$N_THEOREMS" CITV_STUB_PAD="$PAD_BYTES" \
        "$CHECKER" "$PROJ" 2>&1)
  rc=$?
  if [ "$expect" = refuse ]; then
    if [ $rc -ne 0 ] && grep -q -- "$want" <<<"$out"; then
      echo "  refused   $name: '$(grep -m1 -- "$want" <<<"$out" | cut -c1-72)'"
    else
      echo "  FAIL-OPEN $name: exit $rc, and the refusal naming \"$want\" is missing"
      tail -4 <<<"$out" | sed 's/^/            /'
      fails=$((fails + 1))
    fi
  else
    if [ $rc -eq 0 ]; then
      echo "  passed    $name: '$(grep -m1 'RESULT: PASS' <<<"$out" | cut -c1-72)'"
    else
      echo "  BROKEN    $name: the control did not pass (exit $rc), so the refusals above"
      echo "            prove nothing -- every case would refuse whatever the checker did"
      tail -4 <<<"$out" | sed 's/^/            /'
      fails=$((fails + 1))
    fi
  fi
}

echo "== each refusal in $(basename "$CHECKER"), with ${PAD_BYTES}B of build output =="
# The control FIRST: if a clean run does not pass, a refusal means nothing.
case_is clean    pass
case_is replayed refuse "replayed Citv.Axioms from its cache"
case_is notbuilt refuse "did not report building Citv.Axioms"
case_is sorry    refuse "declaration uses sorry"

echo
if [ $fails -eq 0 ]; then
  echo "RESULT: PASS (every site fails closed, and the clean control still passes)"
  exit 0
fi
echo "RESULT: FAIL -- $fails site(s) did not behave"
exit 1
