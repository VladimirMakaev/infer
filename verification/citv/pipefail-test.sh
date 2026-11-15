#!/bin/bash
# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.
#
# Every condition in this tree that reads a pipeline's status must fail CLOSED.
#
# WHY THIS EXISTS.  `producer | grep -q NEEDLE` has two different bugs depending on pipefail,
# and this tree had one of each.
#
#   WITH pipefail (run.sh): grep -q exits at the first match, SIGPIPE kills the producer, and
#   pipefail makes 141 the pipeline's status -- so a SUCCESSFUL match reports failure.  It is
#   size- and position-dependent: measured on this box, a needle at the START of the output
#   answers correctly up to 122KB and wrongly from 152KB.  The cliff is NOT the 64KB pipe
#   buffer; it is a race between two processes, so a test padded to "just past 64KB" passes
#   against the broken form.  lean/check-axioms.sh had three such sites and two of them failed
#   OPEN; run.sh's two were the safe direction only because of a leading `!`.
#
#   WITHOUT pipefail (rocq/run.sh): a producer that exits non-zero is MASKED by a consumer that
#   succeeds.  Those sites were correct only because pipefail was off -- one `set -o pipefail`
#   away from inverting.  Both were changed together, because adding pipefail alone would have
#   inverted them and converting them alone would have left every future pipeline masking.
#
# HOW IT TESTS.  It does not retype the conditions: it EXTRACTS the live text out of the
# shipped scripts and evals it against injected inputs.  Retyped copies drift, and a regression
# test that matches a copy of the code cannot notice the code changing.  Reverting either site
# makes this test pick up the reverted text and fail.
#
#   ./pipefail-test.sh
set -u -o pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RUN=$HERE/run.sh
RRUN=$HERE/rocq/run.sh
PAD_BYTES=200000          # past the measured ~150KB cliff, not the 64KB buffer
fails=0
note () { printf '  %-9s %s\n' "$1" "$2"; }
bad  () { note FAIL "$1"; fails=$((fails + 1)); }

big_with () {   # $1 = needle, placed at the START, then padded past the cliff
  printf '%s\n' "$1"
  head -c "$PAD_BYTES" /dev/zero | tr '\0' 'x'
}

echo "== 1. the live condition text, extracted from the shipped scripts =="
CLASSIFY=$(grep -m1 -F 'if [ -n "$want" ] && ' "$RUN") || true
TOOLCHAIN=$(sed -n '/^if \[ \$dir_rc -eq 2 \]/,/then$/p' "$RRUN") || true
[ -n "$CLASSIFY" ]  || bad "could not find the mutant-classification condition in run.sh"
[ -n "$TOOLCHAIN" ] || bad "could not find the toolchain-refusal condition in rocq/run.sh"
[ $fails -eq 0 ] || { echo; echo "RESULT: FAIL -- the conditions could not be located, so nothing below was tested."; exit 1; }
printf '  %-9s %s\n' "run.sh" "$CLASSIFY"
printf '  %-9s %s\n' "rocq" "$(printf '%s' "$TOOLCHAIN" | tr '\n' ' ' | sed 's/  */ /g')"

echo
echo "== 2. run.sh's classification, driven with a ${PAD_BYTES}B output =="
# The needle IS present, so a correct condition leaves ok=1.  Pre-fix this answered 141, the
# leading `!` turned that into ok=0, and a mutant that WAS correctly refused got reported as
# NOT REFUSED -- a false alarm that fails the suite for the wrong reason.
want='declaration uses sorry'
LAST_OUT=$(big_with "$want"); ok=1
eval "$CLASSIFY"
[ "$ok" = 1 ] && note ok "present needle in a long output -> ok=1 (not inverted)" \
              || bad "present needle in a long output inverted to ok=0"
# Control: absent needle must still set ok=0, or the test above proves only that nothing runs.
LAST_OUT=$(head -c "$PAD_BYTES" /dev/zero | tr '\0' 'x'); ok=1
eval "$CLASSIFY"
[ "$ok" = 0 ] && note ok "absent needle -> ok=0 (the condition still detects a miss)" \
              || bad "absent needle left ok=1: the condition detects nothing"
# Control: an empty want is skipped, which is what the [ -n "$want" ] guard is for.
want=''; LAST_OUT=''; ok=1
eval "$CLASSIFY"
[ "$ok" = 1 ] && note ok "empty want -> skipped" || bad "an empty want was not skipped"

echo
echo "== 3. rocq/run.sh's toolchain refusal, driven with a ${PAD_BYTES}B output =="
# Both halves injected: the REFUSED line must be seen, and a DRIFT line must veto.  The file
# now sets pipefail, so the pre-fix pipe form would invert at this size and send a correct
# refusal down the else branch.
probe_toolchain () {   # $1 = dir_rc, $2 = the output -> prints taken-branch
  local dir_rc=$1
  local dir_out=$2
  local src="$TOOLCHAIN"$'\n'"  echo THEN; else echo ELSE; fi"
  ( set -u -o pipefail; eval "$src" ) 2>/dev/null | tail -1
}
r=$(probe_toolchain 2 "$(big_with 'REFUSED: toolchain missing')")
[ "$r" = THEN ] && note ok "exit 2 + REFUSED line, long output -> accepted" \
                || bad "a genuine toolchain refusal was not recognised (branch=$r)"
r=$(probe_toolchain 2 "$(big_with 'REFUSED: toolchain missing')"$'\nDRIFT: something changed')
[ "$r" = ELSE ] && note ok "a DRIFT line still vetoes it" || bad "the DRIFT veto was lost (branch=$r)"
r=$(probe_toolchain 1 "$(big_with 'REFUSED: toolchain missing')")
[ "$r" = ELSE ] && note ok "wrong exit code still rejected" || bad "exit 1 was accepted (branch=$r)"
r=$(probe_toolchain 2 "$(head -c 200 /dev/zero | tr '\0' 'x')")
[ "$r" = ELSE ] && note ok "no REFUSED line -> rejected" || bad "a missing REFUSED line was accepted (branch=$r)"

echo
echo "== 4. an INJECTED FAILING PRODUCER must fail its pipeline =="
# This is what pipefail buys, and what rocq/run.sh did not have.  The consumer succeeds in
# both cells; only the producer fails.
# `set +o pipefail` explicitly, not merely "don't set it": a subshell INHERITS the option from
# this script, which itself runs under pipefail, so the off cell silently ran with pipefail on
# and both cells agreed. A control that cannot differ from the case it controls is not a
# control -- it only looked like one because it passed.
inject () {
  ( set -u
    if [ "$1" = on ]; then set -o pipefail; else set +o pipefail; fi
    { echo hit; exit 3; } | grep -q hit )
  echo $?
}
a=$(inject on); b=$(inject off)
[ "$a" != 0 ] && note ok "with pipefail: a failing producer fails the pipeline (rc=$a)" \
              || bad "with pipefail a failing producer was masked (rc=$a)"
[ "$b" = 0 ]  && note ok "without pipefail: masked (rc=$b) -- which is why it was added" \
              || bad "the control is wrong: expected masking without pipefail, got rc=$b"
echo "  both scripts set pipefail: run.sh=$(grep -c 'set .*-o pipefail' "$RUN"), rocq/run.sh=$(grep -c 'set .*-o pipefail' "$RRUN")"
[ "$(grep -c 'set .*-o pipefail' "$RRUN")" -ge 1 ] || bad "rocq/run.sh no longer sets pipefail"

echo
echo "== 5. no CONDITION in this tree reads the status of such a pipeline =="
# The dynamic cases above cover the two sites that exist today.  This is what catches the
# THIRD one somebody adds next month.
#
# SCOPED TO CONDITION POSITIONS, and that scoping has a premise.  A display-only pipeline --
# `printf ... | head -5` to show a few lines before exiting -- can also lose its producer to
# SIGPIPE, but nothing reads its status, so nothing changes.  That is true only while no
# script here uses `set -e`, under which a 141 would abort the script.  So the premise is
# asserted below rather than assumed; if someone adds `set -e`, this test says so instead of
# quietly continuing to exclude the cases that would then matter.
scan_conditions () {  # $1 = file; prints "line:text" for each offending condition
  grep -nE '^[[:space:]]*(if|elif|while|until|!|&&|\|\|)[^#]*\|[[:space:]]*(grep -[a-zA-Z]*[qm]|head|sed -n .*q)' "$1" \
    | grep -v '<<<' || true
}
offenders=0; display=0; scripts=0
while IFS= read -r f; do
  scripts=$((scripts + 1))
  while IFS= read -r hit; do
    [ -n "$hit" ] || continue
    echo "    ${f#"$HERE"/}:$hit"
    offenders=$((offenders + 1))
  done < <(scan_conditions "$f")
  d=$(grep -cE '(printf|echo|cat)[^|]*\|[[:space:]]*(head|grep -[a-zA-Z]*[qm])' "$f" || true)
  display=$((display + d))
  if grep -qE '^[[:space:]]*set [^#]*-e([^a-zA-Z]|$)' "$f"; then
    bad "${f#"$HERE"/} uses set -e, so display-only pipelines are no longer harmless here"
  fi
done < <(find "$HERE" -name '*.sh' ! -name 'pipefail-test.sh' | sort)
[ "$offenders" = 0 ] && note ok "0 condition-position pipelines across $scripts scripts" \
                     || bad "$offenders condition(s) above read the status of such a pipeline"
note info "$display display-only pipeline(s) left alone; no script sets -e, so their status is unread"

echo
echo "== 6. vacuity control: the check in 5 must FAIL on a reverted copy =="
# A static check that matches nothing passes forever.  Restore the old form in a scratch copy
# and require the same scan to flag it.
TMP=$(mktemp -d "${TMPDIR:-/tmp}/citv-pipefail.XXXXXXXX") || exit 2
trap 'rm -rf "$TMP"' EXIT
printf '%s\n' '#!/bin/bash' 'set -u -o pipefail' \
  'if [ -n "$want" ] && ! printf '"'"'%s\n'"'"' "$LAST_OUT" | grep -q -- "$want"; then ok=0; fi' \
  > "$TMP/reverted.sh"
n=$(scan_conditions "$TMP/reverted.sh" | grep -c . || true)
[ "$n" -ge 1 ] && note ok "the scan flags a reverted site ($n), so a clean scan means something" \
               || bad "the scan did NOT flag a reverted site: case 5 proves nothing"
# And the second shape, the one rocq/run.sh had, on a continuation line.
printf '%s\n' '#!/bin/bash' 'if [ $rc -eq 2 ] && printf "%s\n" "$o" | grep -q X &&' \
  '   ! printf "%s\n" "$o" | grep -q Y; then :; fi' > "$TMP/reverted2.sh"
n2=$(scan_conditions "$TMP/reverted2.sh" | grep -c . || true)
[ "$n2" -ge 2 ] && note ok "both lines of a two-line condition are flagged ($n2)" \
                || bad "a continuation line escapes the scan ($n2 flagged, wanted 2)"

echo
if [ $fails -eq 0 ]; then
  echo "RESULT: PASS (every condition fails closed, and the scan that proves it is not vacuous)"
  exit 0
fi
echo "RESULT: FAIL -- $fails check(s) did not behave"
exit 1
