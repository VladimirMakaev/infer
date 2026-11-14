#!/bin/bash
# Break the translated module and require the proofs to notice.
#
# A green proof run means nothing unless an unsound module is actually rejected, so each
# mutant is held to two conditions: it must still COMPILE (otherwise "rejected" would only
# mean "broken file"), and Soundness.v must then FAIL.
set -u
cd "$(dirname "$0")" || exit 2
. ./setup.sh || exit 2

MIN_MUTANTS=6
total=0; ok=0
while IFS= read -r line; do
  case "$line" in ''|'#'*) continue;; esac
  name=$(printf '%s' "$line" | cut -d'|' -f1 | sed 's/ *$//')
  target=$(printf '%s' "$line" | cut -d'|' -f2 | tr -d ' ')
  rest=${line#*|*|}
  old=$(printf '%s' "$rest" | awk -F'\\|\\|\\|' '{print $1}' | sed 's/^ *//; s/ *$//')
  new=$(printf '%s' "$rest" | awk -F'\\|\\|\\|' '{print $2}' | sed 's/^ *//; s/ *$//')
  total=$((total+1))

  rm -rf /tmp/citv-mut && cp -r v /tmp/citv-mut
  OLD="$old" NEW="$new" python3 -I -c '
import os,sys,pathlib
p=pathlib.Path(sys.argv[1]); t=p.read_text()
old,new=os.environ["OLD"],os.environ["NEW"]
n=t.count(old)
if n!=1: sys.exit("anchor found %d times, expected 1: %r" % (n, old[:60]))
p.write_text(t.replace(old,new))
' "/tmp/citv-mut/$target" || { echo "  $name: ANCHOR FAILED (the generated text moved)"; continue; }

  ( cd /tmp/citv-mut || exit 2
    for f in IStd IntLit Binop Unop PulseCItv; do
      $ROCQ_RUN rocq compile -Q . "" $f.v > /tmp/citv-mut-build.log 2>&1 || exit 3
    done
    $ROCQ_RUN rocq compile -Q . "" Soundness.v > /tmp/citv-mut-proof.log 2>&1 || exit 4
    exit 0 )
  rc=$?
  case $rc in
    4) echo "  REJECTED (good)  $name"; ok=$((ok+1));;
    3) echo "  BAD  $name: the mutant does not compile, so the rejection would be meaningless";;
    0) echo "  BAD  $name: the proofs STILL PASS on an unsound module";;
    *) echo "  BAD  $name: unexpected rc=$rc";;
  esac
done < mutants.txt

echo "mutants rejected: $ok/$total"
[ "$total" -ge "$MIN_MUTANTS" ] || { echo "REFUSED: only $total mutants, floor is $MIN_MUTANTS"; exit 1; }
[ "$ok" -eq "$total" ] || { echo "REFUSED: $((total-ok)) mutant(s) not rejected"; exit 1; }
exit 0
