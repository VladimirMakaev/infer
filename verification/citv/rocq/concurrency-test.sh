#!/bin/bash
# Two runs at the same time must both be green.
#
# WHAT WENT WRONG.  check-axioms.sh and mutate.sh wrote fixed paths -- /tmp/citv-expected.txt,
# /tmp/citv-actual.txt, /tmp/citv-axioms.log and a whole mutant tree at /tmp/citv-mut -- so a
# reviewer running the proofs while the standing check fired had the two runs writing the same
# files.  The mutant tree is the bad one: one run deletes and repopulates it while the other is
# compiling inside it, so the second run reports "the mutant does not compile", which reads as
# a real result about the mutant rather than as interference.  A reviewer hit this and had to
# serialise by hand.
#
# WHAT THIS DOES NOT CLAIM.  Two runs in the SAME directory are still unsafe, and no choice of
# temp path can fix that: the proofs are compiled in v/ itself, which check-axioms.sh cleans
# first, so a shared checkout races on .vo files.  The case this is about is two CHECKOUTS --
# a reviewer's worktree and the standing check's -- which shared nothing but /tmp.  The peer
# copy below models exactly that, and it is the honest scope of the fix.
#
#   ./concurrency-test.sh            check-axioms.sh and mutate.sh, this copy against a peer
#   ./concurrency-test.sh --axioms   the quick half: check-axioms.sh only
set -u
cd "$(dirname "$0")" || exit 2
. ./setup.sh || exit 2

QUICK=0
[ "${1:-}" = --axioms ] && QUICK=1

PEER=$(mktemp -d "${TMPDIR:-/tmp}/citv-peer.XXXXXXXX") || exit 2
trap 'rm -rf "$PEER"' EXIT

# An independent copy: its own v/, its own expected/, so the only thing the two runs can still
# collide on is a temp path -- which is the thing under test.
cp -r . "$PEER/rocq" || { echo "could not make the peer copy"; exit 2; }
rm -f "$PEER"/rocq/v/*.vo "$PEER"/rocq/v/*.vok "$PEER"/rocq/v/*.vos "$PEER"/rocq/v/*.glob

fail=0
pair() {  # pair SCRIPT ARGS...
  local script=$1; shift
  echo "== two $script at once"
  ( cd "$PEER/rocq" && ./"$script" "$@" ) > "$PEER/peer.log" 2>&1 &
  local peer_pid=$!
  ./"$script" "$@" > "$PEER/self.log" 2>&1
  local self_rc=$?
  wait $peer_pid; local peer_rc=$?
  local who
  for who in self peer; do
    local rc; [ $who = self ] && rc=$self_rc || rc=$peer_rc
    if [ "$rc" -eq 0 ]; then
      echo "  ok       $who: exit 0, '$(grep -m1 -E '^PASS|^mutants rejected' "$PEER/$who.log")'"
    else
      echo "  FAILED   $who: exit $rc"
      tail -15 "$PEER/$who.log" | sed 's/^/           /'
      fail=1
    fi
  done
}

# check-axioms.sh needs ROCQ_RUN from its caller and does not source setup.sh itself; this
# script did, so both halves of each pair inherit it.
pair check-axioms.sh
[ $QUICK -eq 1 ] || pair mutate.sh

if [ $fail -eq 0 ]; then
  echo "CONCURRENCY OK: both runs green"
  exit 0
fi
echo "CONCURRENCY FAILED: a simultaneous run was not green"
exit 1
