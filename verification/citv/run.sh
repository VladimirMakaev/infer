#!/bin/bash
# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.
#
# Bounded soundness check for infer/src/pulse/PulseCItv.ml. See README.md.
#
#   ./verification/citv/run.sh                check the module, then kill every mutant
#   ./verification/citv/run.sh --oracle       check the module only
#   ./verification/citv/run.sh --retired      replay the retired mutants and check their record
#   ./verification/citv/run.sh --adversarial  attack every guard; each edit must be refused
#   ./verification/citv/run.sh --window W T   endpoints in [-W,W], integers checked over [-T,T]
#
# Nothing here is part of Infer's build: this directory is outside the dune root (infer/), so
# `make` and `dune build` never see it. The module under test is read from the working tree at
# run time, so this directory cannot hold a stale fork of it.

set -u -o pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SUBJECT_ML="$ROOT/infer/src/pulse/PulseCItv.ml"
SUBJECT_MLI="$ROOT/infer/src/pulse/PulseCItv.mli"

MODE=all
W=4
T=8

# A loop over an empty file runs no iterations and reports no failures, which reads exactly
# like a pass. These are floors, not counts: adding a mutant needs no edit here, removing one
# does. They are the same idea as the cardinalities declared in the shims, for the same reason.
MIN_MUTANTS=33
MIN_RETIRED=10
MIN_ADVERSARIAL=18
while [ $# -gt 0 ]; do
  case "$1" in
    --oracle) MODE=oracle; shift ;;
    --retired) MODE=retired; shift ;;
    --adversarial) MODE=adversarial; shift ;;
    --window) W="$2"; T="$3"; shift 3 ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done

for f in "$SUBJECT_ML" "$SUBJECT_MLI"; do
  [ -f "$f" ] || { echo "cannot find $f -- run this from a checkout" >&2; exit 2; }
done
command -v dune >/dev/null || {
  echo "dune is not on PATH. This check needs an OCaml switch with:" >&2
  echo "  opam install dune zarith yojson ppx_compare ppx_yojson_conv" >&2
  exit 2
}

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

# Assemble a throwaway project: this directory's own files, plus the module under test copied
# out of the working tree. The copy lives only in $WORK and its checksum is printed, so the
# bytes checked are provably the bytes in the tree.
cp -r "$HERE/dune-project" "$HERE/shims" "$HERE/module" "$HERE/probe" "$HERE/oracle" "$WORK/"
restore_subject() {
  cp "$SUBJECT_ML" "$WORK/module/PulseCItv.ml"
  cp "$SUBJECT_MLI" "$WORK/module/PulseCItv.mli"
  cp "$SUBJECT_ML" "$WORK/probe/PulseCItv.ml"
}
restore_subject

echo "subject:  infer/src/pulse/PulseCItv.ml"
echo "sha256:   $(sha256sum "$SUBJECT_ML" | cut -d' ' -f1)"
echo "sha256:   $(sha256sum "$SUBJECT_MLI" | cut -d' ' -f1)  (interface)"
echo

build_oracle() { ( cd "$WORK" && dune build ./oracle/oracle.exe 2>&1 ); }

run_oracle() { ( cd "$WORK" && dune exec --no-print-directory ./oracle/oracle.exe -- "$W" "$T" 2>&1 ); }

# Apply one edit to a list of files. Fails if the anchor is not unique in any of them, so a
# mutant left behind by a change to the module is reported rather than silently skipped.
apply_edit() {
  local old="$1" new="$2"; shift 2
  python3 - "$old" "$new" "$@" <<'PY'
import sys, pathlib
old, new, paths = sys.argv[1].replace("\\n", "\n"), sys.argv[2].replace("\\n", "\n"), sys.argv[3:]
for path in paths:
    p = pathlib.Path(path)
    s = p.read_text()
    if s.count(old) != 1:
        sys.exit(f"edit anchor is not unique ({s.count(old)} matches) in {p.name}")
    p.write_text(s.replace(old, new))
PY
}

apply_mutant() {
  apply_edit "$1" "$2" "$WORK/module/PulseCItv.ml" "$WORK/probe/PulseCItv.ml"
}

# What happened to the mutated module: did not build, built and was judged clean, built and was
# caught, or built and took the process down before the oracle could report. The oracle counts an
# exception escaping the module as a violation in its own right, so "crashed" here means the
# process itself died, which is never a kill.
OUTCOME=""
DETAIL=""
LAST_OUT=""
classify_mutant() {
  local out status
  out="$(build_oracle)"
  if [ $? -ne 0 ]; then
    OUTCOME=no-compile
    LAST_OUT="$out"
    DETAIL="$(printf '%s\n' "$out" | grep -m1 -E '^(Error|File)' | cut -c1-70)"
    return
  fi
  out="$(run_oracle)"
  status=$?
  LAST_OUT="$out"
  local n wrong raised
  n="$(printf '%s\n' "$out" | sed -n 's/^soundness violations: //p')"
  wrong="$(printf '%s\n' "$out" | sed -n 's/^  wrong answers: *//p')"
  raised="$(printf '%s\n' "$out" | sed -n 's/^  escaping exceptions: *//p')"
  if [ -z "$n" ]; then
    OUTCOME=crashed
    DETAIL="$(printf '%s\n' "$out" | tail -1 | cut -c1-70)"
  elif [ "$n" = "0" ]; then
    OUTCOME=survived
    DETAIL="exit $status"
  else
    OUTCOME=killed
    DETAIL="$wrong wrong, $raised raised"
  fi
}

echo "== the module as it stands in the tree =="
OUT="$(run_oracle)"
STATUS=$?
echo "$OUT"
if [ $STATUS -ne 0 ]; then
  echo
  echo "RESULT: FAIL -- the module in the tree violates a property above."
  exit 1
fi

if [ $MODE = oracle ]; then
  echo
  echo "RESULT: PASS (oracle only; mutants not run)"
  exit 0
fi

if [ $MODE = adversarial ]; then
  # Every guard in this directory, attacked. Three rounds of review each found the next domain
  # the oracle silently did not visit, so the guards are now themselves subjects: each row below
  # is an edit that must be refused, and the run fails if any is accepted. The attacks are the
  # ones that keep working -- truncate at the start, at the end, in the middle, make it empty,
  # make it repeat, and stop one short of a bound.
  echo
  echo "== every guard, attacked: each edit must be refused =="
  ADV_FAILS=0
  ADV_RUN=0
  adv_case() {
    # name expected want file old new [file2 old2 new2]
    local name="$1" expected="$2" want="$3" f="$4" old="$5" new="$6"
    local f2="${7:-}" old2="${8:-}" new2="${9:-}"
    ADV_RUN=$((ADV_RUN + 1))
    local ok=1
    if ! apply_edit "$old" "$new" "$WORK/$f"; then
      echo "  ANCHOR LOST  $name (in $f)"
      ok=0
    elif [ -n "$f2" ] && ! {
      if [ "$f2" = module/PulseCItv.ml ]; then apply_mutant "$old2" "$new2";
      else apply_edit "$old2" "$new2" "$WORK/$f2"; fi
    }; then
      echo "  ANCHOR LOST  $name (in $f2)"
      ok=0
    else
      classify_mutant
      [ "$OUTCOME" = "$expected" ] || ok=0
      if [ -n "$want" ] && ! printf '%s\n' "$LAST_OUT" | grep -q -- "$want"; then ok=0; fi
      if [ $ok -eq 1 ]; then
        echo "  refused      $name: $OUTCOME ($DETAIL)"
      else
        echo "  NOT REFUSED  $name: wanted $expected${want:+ with \"$want\"}, got $OUTCOME ($DETAIL)"
      fi
    fi
    [ $ok -eq 1 ] || ADV_FAILS=$((ADV_FAILS + 1))
    cp "$HERE/shims/Binop.ml" "$WORK/shims/Binop.ml"
    cp "$HERE/shims/Unop.ml" "$WORK/shims/Unop.ml"
    cp "$HERE/oracle/oracle.ml" "$WORK/oracle/oracle.ml"
    restore_subject
  }

  # --- the operator enumeration: dropping a shape from either match is a compile error ------
  adv_case "succ: drop a payload shape" no-compile "" shims/Binop.ml \
    '  | PlusA (Some ()) ->\n      Some PlusPI\n' ''
  adv_case "stop deriving the domain from the type" no-compile "" shims/Binop.ml \
    '[@@deriving compare, equal, enumerate]' '[@@deriving compare, equal]'

  # --- the operator enumeration: truncations the compiler cannot see ------------------------
  # Each of these leaves both matches exhaustive, so only the cardinality and the two terminals
  # declared in the shim can refuse them. An attestation that asked the list for its own size
  # would accept every one.
  adv_case "Binop: truncate at the end" killed "[domain]" shims/Binop.ml \
    '  | LAnd ->\n      Some LOr\n' '  | LAnd ->\n      None\n'
  adv_case "Unop: truncate at the end" killed "[domain]" shims/Unop.ml \
    'match u with Neg -> Some BNot | BNot -> Some LNot | LNot -> None' \
    'match u with Neg -> Some BNot | BNot -> None | LNot -> None'
  adv_case "Binop: truncate at the start" killed "[domain]" shims/Binop.ml \
    '  go (PlusA None)' '  go (PlusA (Some ()))'
  adv_case "Binop: truncate in the interior" killed "[domain]" shims/Binop.ml \
    '  | Mult None ->\n      Some (Mult (Some ()))\n' '  | Mult None ->\n      Some DivI\n'
  adv_case "Binop: enumerate nothing at all" killed "[domain]" shims/Binop.ml \
    '  go (PlusA None)' '  ignore (go (PlusA None) : t list) ;\n  []'
  adv_case "Binop: visit a shape twice" killed "produced more than once" shims/Binop.ml \
    '  go (PlusA None)' '  PlusA None :: go (PlusA None)'

  # The subject that made the self-referential attestation visible: truncate the enumeration at
  # the end and hide a wrong answer in the shape that falls off it. Before the cardinality was
  # declared independently, this exited 0 with every mutant still killed.
  adv_case "Binop: a wrong LOr hidden behind a truncation" killed "[domain]" \
    shims/Binop.ml '  | LAnd ->\n      Some LOr\n' '  | LAnd ->\n      None\n' \
    module/PulseCItv.ml '  | Mod ->\n      literal_of a_rhs |> Option.bind ~f:(rem_by_literal a_lhs)\n  | PlusPI\n  | MinusPI\n  | MinusPP\n  | Mult _\n  | DivF\n  | Shiftlt\n  | Shiftrt\n  | Lt\n  | Gt\n  | Le\n  | Ge\n  | Eq\n  | Ne\n  | BAnd\n  | BXor\n  | BOr\n  | LAnd\n  | LOr ->\n      None\n' \
    '  | Mod ->\n      literal_of a_rhs |> Option.bind ~f:(rem_by_literal a_lhs)\n  | LOr ->\n      Some (equal_to IntLit.zero)\n  | PlusPI\n  | MinusPI\n  | MinusPP\n  | Mult _\n  | DivF\n  | Shiftlt\n  | Shiftrt\n  | Lt\n  | Gt\n  | Le\n  | Ge\n  | Eq\n  | Ne\n  | BAnd\n  | BXor\n  | BOr\n  | LAnd ->\n      None\n'

  # --- the operand values: the domain nothing attested until round five --------------------
  # The sampler builds the value space by calling the module's constructors. Dropping one
  # interval from it leaves every other guard happy: the space is not empty, every property
  # still runs, and the totals are merely a little smaller. Only set equality against an
  # expectation written straight from the integers can see it.
  VALUES='  let bounded = pairs "between" (fun a b -> C.between (B.Int (z a)) (B.Int (z b))) in'
  drop_value() {
    # name  predicate-on-(a,b) to EXCLUDE
    adv_case "values: $1" killed "[domain]" oracle/oracle.ml "$VALUES" \
      '  let bounded =\n    List.filteri (fun i _ -> not ('"$2"')) (pairs "between" (fun a b -> C.between (B.Int (z a)) (B.Int (z b))))\n  in'
  }
  drop_value "drop the first interval"    'i = 0'
  drop_value "drop the last interval"     'i = List.length (pairs "between" (fun a b -> C.between (B.Int (z a)) (B.Int (z b)))) - 1'
  drop_value "drop an interval in the middle" 'i = 1'
  adv_case "values: produce one twice" killed "[domain]" oracle/oracle.ml "$VALUES" \
    '  let bounded =\n    let p = pairs "between" (fun a b -> C.between (B.Int (z a)) (B.Int (z b))) in\n    List.hd p :: p\n  in'
  # The round-four subject: drop only Between(W,W), and hide a wrong answer for {W}+{W} in it.
  adv_case "values: a wrong 4+4 hidden behind a dropped Between(W,W)" killed "[domain]" \
    oracle/oracle.ml "$VALUES" \
    '  let bounded =\n    List.filter\n      (fun v -> not (C.equal v (C.between (B.Int (z w)) (B.Int (z w)))))\n      (pairs "between" (fun a b -> C.between (B.Int (z a)) (B.Int (z b))))\n  in' \
    module/PulseCItv.ml '  | PlusA _ ->\n      add a_lhs a_rhs' \
    '  | PlusA _ ->\n      if\n        match (a_lhs, a_rhs) with\n        | Between (Int l1, Int u1), Between (Int l2, Int u2) ->\n            IntLit.eq l1 u1 && IntLit.eq l2 u2 && IntLit.eq l1 (IntLit.of_int 4)\n            && IntLit.eq l2 (IntLit.of_int 4)\n        | _ ->\n            false\n      then Some (equal_to IntLit.zero)\n      else add a_lhs a_rhs'

  # --- the literal set ---------------------------------------------------------------------
  adv_case "literals: judge one integer fewer" killed "[domain]" oracle/oracle.ml \
    'let concrete ~t = List.init ((2 * t) + 1) (fun i -> i - t)' \
    'let concrete ~t = List.init (2 * t) (fun i -> i - t)'

  # --- a numeric bound: the shift count ----------------------------------------------------
  # The concrete evaluator used to stop judging shift counts above 16, so a module that is
  # right below a machine word and wrong above it was accepted. The arithmetic is arbitrary
  # precision now and nothing is bounded. This subject shows the difference: it is correct over
  # the default window and wrong only past 2^16, so it has to be run at a window that reaches
  # there, which is the point -- the old evaluator called 17 undefined and passed it.
  adv_subject_case() {
    local name="$1" expected="$2" want="$3" ww="$4" tt="$5" old="$6" new="$7"
    local saved_w="$W" saved_t="$T" ok=1
    W="$ww"; T="$tt"
    ADV_RUN=$((ADV_RUN + 1))
    if ! apply_mutant "$old" "$new"; then
      echo "  ANCHOR LOST  $name"
      ok=0
    else
      classify_mutant
      [ "$OUTCOME" = "$expected" ] || ok=0
      if [ -n "$want" ] && ! printf '%s\n' "$LAST_OUT" | grep -q -- "$want"; then ok=0; fi
      if [ $ok -eq 1 ]; then
        echo "  refused      $name (at window $ww $tt): $OUTCOME ($DETAIL)"
      else
        echo "  NOT REFUSED  $name (at window $ww $tt): wanted $expected, got $OUTCOME ($DETAIL)"
      fi
    fi
    [ $ok -eq 1 ] || ADV_FAILS=$((ADV_FAILS + 1))
    W="$saved_w"; T="$saved_t"
    restore_subject
  }
  adv_subject_case "Shiftlt: right below a machine word, wrong above it" killed "Shiftlt" 1 17 \
    '  | Mod ->\n      literal_of a_rhs |> Option.bind ~f:(rem_by_literal a_lhs)\n  | PlusPI\n  | MinusPI\n  | MinusPP\n  | Mult _\n  | DivF\n  | Shiftlt\n  | Shiftrt' \
    '  | Mod ->\n      literal_of a_rhs |> Option.bind ~f:(rem_by_literal a_lhs)\n  | Shiftlt ->\n      Some (between Bound.MinusInfinity (Bound.Int (IntLit.of_int 2000000)))\n  | PlusPI\n  | MinusPI\n  | MinusPP\n  | Mult _\n  | DivF\n  | Shiftrt'

  # --- the property table: a check whose loop never runs leaves no trace --------------------
  adv_case "run a property's loop over nothing" killed "never exercised" oracle/oracle.ml \
    '  List.iter (fun a -> List.iter (fun b -> check_intersection ~t a b) somes) somes ;' \
    '  List.iter (fun a -> List.iter (fun b -> check_intersection ~t a b) []) somes ;'
  adv_case "empty the value space" killed "[domain]" oracle/oracle.ml \
    '  let somes = List.filter_map (fun v -> v) vs in' \
    '  let somes = [] in\n  ignore (List.filter_map (fun v -> v) vs : C.t list) ;'

  echo
  if [ $ADV_FAILS -ne 0 ]; then
    echo "RESULT: FAIL -- $ADV_FAILS of $ADV_RUN edits was accepted; a guard is not guarding."
    exit 1
  fi
  if [ $ADV_RUN -lt $MIN_ADVERSARIAL ]; then
    echo "RESULT: FAIL -- only $ADV_RUN attacks ran and at least $MIN_ADVERSARIAL are expected;"
    echo "the table of attacks has itself been truncated."
    exit 1
  fi
  echo "RESULT: PASS (all $ADV_RUN attacks refused)"
  exit 0
fi

if [ $MODE = retired ]; then
  # Every mutant that was tried and is not in mutants.txt, with the outcome it produced at the
  # time. Replaying them checks the record in README.md instead of asking the reader to take it
  # on trust, and catches the case where a later strengthening of the oracle turns a recorded
  # survivor into a kill.
  echo
  echo "== retired mutants: each must reproduce its recorded outcome =="
  N_RETIRED="$(grep -c "$(printf '\t')" "$HERE/mutants/retired.txt")"
  if [ "$N_RETIRED" -lt "$MIN_RETIRED" ]; then
    echo "RESULT: FAIL -- retired.txt has $N_RETIRED entries and at least $MIN_RETIRED are expected."
    exit 1
  fi
  WRONG=0
  SEEN=0
  MISMATCHES=""
  while IFS=$'\t' read -r NAME EXPECTED OLD NEW; do
    case "$NAME" in ''|'#'*) continue ;; esac
    if ! apply_mutant "$OLD" "$NEW"; then
      echo "  ANCHOR LOST $NAME (the module has changed under it; the record cannot be replayed)"
      WRONG=$((WRONG + 1))
      MISMATCHES="$MISMATCHES $NAME"
    else
      classify_mutant
      if [ "$OUTCOME" = "$EXPECTED" ]; then
        echo "  as recorded  $NAME: $OUTCOME ($DETAIL)"
      else
        echo "  MISMATCH     $NAME: recorded $EXPECTED, replayed $OUTCOME ($DETAIL)"
        WRONG=$((WRONG + 1))
        MISMATCHES="$MISMATCHES $NAME"
      fi
    fi
    SEEN=$((SEEN + 1))
    restore_subject
  done < "$HERE/mutants/retired.txt"
  echo
  if [ $WRONG -ne 0 ]; then
    echo "RESULT: FAIL -- the record is wrong for:$MISMATCHES"
    exit 1
  fi
  if [ "$SEEN" -ne "$N_RETIRED" ]; then
    echo "RESULT: FAIL -- $N_RETIRED entries are in the file but $SEEN were replayed."
    exit 1
  fi
  echo "RESULT: PASS ($SEEN of $N_RETIRED reproduced their recorded outcome)"
  exit 0
fi

# Each mutant is a single edit to one helper that makes it unsound. A mutant the oracle does
# not kill is a gap in the oracle, so an unkilled mutant fails this script, and so does one that
# fails to build or takes the process down, because in neither case did the oracle judge it.
echo
echo "== mutants: each must be caught =="
N_MUTANTS="$(grep -c "$(printf '\t')" "$HERE/mutants/mutants.txt")"
if [ "$N_MUTANTS" -lt "$MIN_MUTANTS" ]; then
  echo "RESULT: FAIL -- mutants.txt has $N_MUTANTS mutants and at least $MIN_MUTANTS are expected."
  exit 1
fi
KILLED=0
SURVIVED=0
SURVIVORS=""
while IFS=$'\t' read -r NAME OLD NEW; do
  case "$NAME" in ''|'#'*) continue ;; esac
  if ! apply_mutant "$OLD" "$NEW"; then
    echo "  ANCHOR LOST $NAME (the module has changed under it; the mutant needs updating)"
    SURVIVED=$((SURVIVED + 1))
    SURVIVORS="$SURVIVORS $NAME"
  else
    classify_mutant
    case "$OUTCOME" in
      killed)
        echo "  killed     $NAME ($DETAIL)"
        KILLED=$((KILLED + 1))
        ;;
      *)
        echo "  NOT A KILL $NAME: $OUTCOME ($DETAIL)"
        SURVIVED=$((SURVIVED + 1))
        SURVIVORS="$SURVIVORS $NAME"
        ;;
    esac
  fi
  restore_subject
done < "$HERE/mutants/mutants.txt"

echo
echo "mutants killed: $KILLED, not killed: $SURVIVED, in the file: $N_MUTANTS"
if [ $SURVIVED -ne 0 ]; then
  echo "RESULT: FAIL -- unkilled mutants:$SURVIVORS"
  exit 1
fi
if [ $((KILLED + SURVIVED)) -ne "$N_MUTANTS" ]; then
  echo "RESULT: FAIL -- $N_MUTANTS mutants are in the file but $((KILLED + SURVIVED)) were run."
  exit 1
fi
echo "RESULT: PASS ($KILLED of $N_MUTANTS killed)"
