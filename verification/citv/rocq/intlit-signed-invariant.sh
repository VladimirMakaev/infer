#!/bin/bash
# The premise the ported theorems rest on: no negative UNSIGNED IntLit is ever created.
#
# Every theorem in v/Soundness.v about abduce_lt, the flip and the dispatch is conditional
# on its literals being outside area 3 - [nnu] there, "Signed or non-negative".  That
# hypothesis is free in the analyser, and this script is why anyone is entitled to say so.
#
# WHAT ROCQ CAN AND CANNOT SEE.  The CLOSURE half is machine-checked: signed_literals_are
# _closed, sgn_add, sgn_neg and sgn_itv_minus say the operations stay in the fragment.  The
# BASE CASE is not and cannot be, because the development compiles a STAND-IN IntLit
# (stubs/intLit.ml), not infer/src/IR/IntLit.ml, whose real constructors are of_z and null.
# So the base case is pinned here, against the real module.
#
# THE LOAD-BEARING ARGUMENT IS NOT A GREP.  IntLit.mli declares [type t] abstract and does
# not mention the type [signedness] at all, so no other module can even NAME the
# constructor Unsigned - that is the OCaml type system, not a pattern match on text.  The
# checks below pin the premises of THAT argument: that the interface stays abstract (C3),
# that the module itself writes only Signed into a record (C1, C2), and that the one
# derived back door the interface does open has no caller (C4).  C5 is a redundant canary,
# kept because it is one line and costs nothing, not because the argument needs it.
#
# WHY NOT A sha256 PIN, which is how expected/source.sha256 guards PulseCItv.ml.  IntLit.ml
# is edited for reasons that have nothing to do with signedness, so a content pin would
# refuse on every unrelated change, and a check that cries wolf is relaxed and then
# deleted.  These checks refuse exactly when the invariant could have moved.
#
# COMMENTS ARE STRIPPED BEFORE MATCHING.  A keyword gate that matches its own
# documentation fires on the comment explaining the keyword, and the next person removes
# the gate rather than the comment.  See strip-ocaml-comments.py.
#
#   ./intlit-signed-invariant.sh              check the tree this file is in
#   ./intlit-signed-invariant.sh --self-test  plant one violation per check and require a
#                                             refusal that NAMES that check
#
# Exit 0 pass, 1 a check failed, 2 the check could not be carried out.  CITV_INTLIT_ROOT
# overrides the tree that is read; only the self-test sets it.
set -u
export LC_ALL=C
cd "$(dirname "$0")" || exit 2
HERE=$PWD

STRIP="$HERE/strip-ocaml-comments.py"
[ -f "$STRIP" ] || { echo "REFUSED: toolchain -- $STRIP is missing"; exit 2; }

# --------------------------------------------------------- comment-aware tree scanning
# TWO STAGES, and the first one is only a filter.  An ordinary grep over infer/src names
# the candidate files in milliseconds; the stripper then re-reads just those with comments
# and string literals blanked, and ITS answer is the verdict.  Doing only the first stage
# is how the first version of this script fired on a comment that said the keyword, which
# its own self-test caught.  Doing only the second would strip several thousand files.
scan () { # scan <root> <extended-regex> -> path:lineno:line over the stripped text
  local root=$1 pat=$2 cand
  cand=$(grep -rlE "$pat" "$root/infer/src" --include='*.ml' --include='*.mli' 2>/dev/null)
  [ -n "$cand" ] || return 0
  # shellcheck disable=SC2086
  python3 -I "$STRIP" --grep "$pat" $cand 2>/dev/null
  return 0
}

scan_file () { # scan_file <path> <extended-regex>
  python3 -I "$STRIP" --grep "$2" "$1" 2>/dev/null
  return 0
}

# ----------------------------------------------------------------------- the checks
run_checks () { # run_checks <root>
  local ROOT=$1 FAIL=0 TMP
  local ML="$ROOT/infer/src/IR/IntLit.ml" MLI="$ROOT/infer/src/IR/IntLit.mli"
  bad () { printf 'REFUSED (%s): %s\n' "$1" "$2"; FAIL=1; }

  for f in "$ML" "$MLI"; do
    [ -f "$f" ] || { echo "REFUSED: toolchain -- $f does not exist"; return 2; }
  done
  TMP=$(mktemp -d "${TMPDIR:-/tmp}/citv-intlit.XXXXXXXX") || return 2

  python3 -I "$STRIP" "$ML" > "$TMP/ml.txt" || { echo "REFUSED: toolchain -- stripper failed"; return 2; }
  python3 -I "$STRIP" "$MLI" > "$TMP/mli.txt" || { echo "REFUSED: toolchain -- stripper failed"; return 2; }

  # --- C1 unsigned-sites.  Set equality in BOTH directions: a new place that mentions the
  # constructor fails, and so does a pinned one that disappears, so the pin cannot quietly
  # fall behind the file the way a one-directional count would.
  grep -F 'Unsigned' "$TMP/ml.txt" | sed 's/^ *//; s/ *$//' | sort -u > "$TMP/c1.got"
  sort -u expected/intlit-unsigned-sites.txt > "$TMP/c1.want"
  if ! diff -u "$TMP/c1.want" "$TMP/c1.got" > "$TMP/c1.diff"; then
    bad unsigned-sites "the places IntLit.ml mentions Unsigned have changed."
    sed 's/^/      /' "$TMP/c1.diff"
    echo "      -> if a new site cannot put Unsigned into a t that was not already Unsigned,"
    echo "         add its line to expected/intlit-unsigned-sites.txt.  If it can, the nnu"
    echo "         hypothesis in v/Soundness.v no longer covers the reachable inputs."
  else
    echo "ok 1/5  unsigned-sites: IntLit.ml mentions Unsigned in the $(wc -l < "$TMP/c1.want") pinned places"
  fi

  # --- C2 construction-sites.  Every record literal that writes the signedness field.
  grep -F 'signedness=' "$TMP/ml.txt" | sed 's/^ *//; s/ *$//' | sort -u > "$TMP/c2.got"
  sort -u expected/intlit-construction-sites.txt > "$TMP/c2.want"
  if ! diff -u "$TMP/c2.want" "$TMP/c2.got" > "$TMP/c2.diff"; then
    bad construction-sites "the places IntLit.ml writes a signedness field have changed."
    sed 's/^/      /' "$TMP/c2.diff"
    echo "      -> two of the pinned lines are the only record CONSTRUCTIONS, of_z and null,"
    echo "         and both write the literal Signed; the others are patterns and the join."
  else
    echo "ok 2/5  construction-sites: the $(wc -l < "$TMP/c2.want") pinned signedness fields are unchanged"
  fi

  # --- C3 interface-is-abstract.  This is the one the whole argument hangs on: with
  # [signedness] absent from the interface, no other module can write the constructor even
  # with an open, because the name does not exist outside IntLit.ml.
  local leak
  leak=$(grep -nE 'signedness|Unsigned|pointerness' "$TMP/mli.txt")
  if [ -n "$leak" ]; then
    bad interface-is-abstract "IntLit.mli exposes the signedness tag:"
    sed 's/^/      /' <<<"$leak"
  elif grep -qE '^type t *=' "$TMP/mli.txt"; then
    bad interface-is-abstract "IntLit.mli defines type t rather than leaving it abstract:"
    grep -nE '^type t *=' "$TMP/mli.txt" | sed 's/^/      /'
  else
    echo "ok 3/5  interface-is-abstract: IntLit.mli keeps t abstract and never names the tag"
  fi

  # --- C4 sexp-entry-has-no-caller.  [@@deriving sexp] on the abstract t exports
  # t_of_sexp, which builds a record from arbitrary input and is therefore the one way
  # round C3.  Two halves: no direct caller, and the set of types that could reach it
  # through their OWN derivation is pinned, so a new one has to be looked at.
  local direct
  direct=$(scan "$ROOT" 'IntLit\.t_of_sexp|of_sexp: *IntLit\.t\b|Typ\.(t|desc)_of_sexp')
  if [ -n "$direct" ]; then
    bad sexp-entry-has-no-caller "something deserialises an IntLit:"
    sed "s|$ROOT/||; s/^/      /" <<<"$direct"
  else
    scan "$ROOT" 'IntLit\.t\b' | cut -d: -f1 | sort -u > "$TMP/c4.cand"
    : > "$TMP/c4.got"
    while read -r f; do
      [ -n "$(scan_file "$f" 'deriving[^]]*sexp')" ] && printf '%s\n' "${f#"$ROOT"/}" >> "$TMP/c4.got"
    done < "$TMP/c4.cand"
    sort -u -o "$TMP/c4.got" "$TMP/c4.got"
    sort -u expected/intlit-sexp-reachable-files.txt > "$TMP/c4.want"
    if ! diff -u "$TMP/c4.want" "$TMP/c4.got" > "$TMP/c4.diff"; then
      bad sexp-entry-has-no-caller "the set of sexp-deriving types that mention IntLit.t has changed."
      sed 's/^/      /' "$TMP/c4.diff"
      echo "      -> each one derives a *_of_sexp that can reach IntLit.t_of_sexp.  Confirm"
      echo "         the new one has no caller, then add it to"
      echo "         expected/intlit-sexp-reachable-files.txt."
    else
      echo "ok 4/5  sexp-entry-has-no-caller: no caller, and the $(wc -l < "$TMP/c4.want") derived chains are pinned"
    fi
  fi

  # --- C5 no-other-module-names-the-tag.  Redundant given C3 - the name is not in scope
  # outside IntLit.ml, so this cannot fail while C3 passes - and kept as a canary that
  # costs one grep.  It is NOT part of the argument; do not let it become the argument.
  local others
  others=$(scan "$ROOT" 'open!? +IntLit\b'
           scan "$ROOT" 'Unsigned' | grep -v '^[^:]*/IntLit\.mli\?:' | grep 'IntLit')
  if [ -n "$others" ]; then
    bad no-other-module-names-the-tag "another module names IntLit's signedness tag:"
    sed "s|$ROOT/||; s/^/      /" <<<"$others"
  else
    echo "ok 5/5  no-other-module-names-the-tag: nothing outside IntLit.ml names it"
  fi

  rm -rf "$TMP"
  return $FAIL
}

# ------------------------------------------------------------------------- self-test
# A guard nobody has watched refuse is not a guard.  Each plant below must make the check
# it targets REFUSE BY NAME; it is not enough that the script exits non-zero, because a
# script that refused for an unrelated reason would pass that weaker test.  The inert copy
# is checked FIRST: a control that cannot pass proves nothing about the plants.
self_test () {
  local ROOT REAL BASE rc out pass=0 fail=0
  REAL=${CITV_INTLIT_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null)} || true
  [ -n "${REAL:-}" ] || { echo "REFUSED: toolchain -- not in a git tree"; return 2; }
  BASE=$(mktemp -d "${TMPDIR:-/tmp}/citv-intlit-selftest.XXXXXXXX") || return 2
  trap 'rm -rf "$BASE"' RETURN

  mkdir -p "$BASE/pristine/infer/src/IR"
  for f in IntLit.ml IntLit.mli Typ.ml Typ.mli; do
    cp "$REAL/infer/src/IR/$f" "$BASE/pristine/infer/src/IR/$f" || return 2
  done

  echo "  --- the inert copy must PASS before any plant means anything"
  out=$(run_checks "$BASE/pristine"); rc=$?
  if [ $rc -eq 0 ]; then
    echo "  ok        inert copy passes"; pass=$((pass+1))
  else
    echo "  BAD       the unplanted copy already fails, so no plant below proves anything:"
    sed 's/^/      /' <<<"$out"; fail=$((fail+1))
  fi

  plant () { # plant <label> <expected-check-name> <shell that edits $R>
    local label=$1 want=$2 edit=$3 R
    R="$BASE/$label"
    rm -rf "$R" && cp -r "$BASE/pristine" "$R"
    ( cd "$R" && eval "$edit" ) || { echo "  BAD       $label: the plant itself failed"; fail=$((fail+1)); return; }
    out=$(run_checks "$R"); rc=$?
    if [ $rc -eq 0 ]; then
      printf '  SURVIVED  %-34s the checks still passed\n' "$label"; fail=$((fail+1)); return
    fi
    if grep -q "REFUSED ($want)" <<<"$out"; then
      printf '  refused   %-34s by %s\n' "$label" "$want"; pass=$((pass+1))
    else
      printf '  BAD       %-34s refused, but NOT by %s:\n' "$label" "$want"
      grep -m2 '^REFUSED' <<<"$out" | sed 's/^/      /'; fail=$((fail+1))
    fi
  }

  # The other half of "comments are stripped": these put the keywords in a COMMENT only,
  # and the checks must still PASS.  Without this the stripper could be deleted and every
  # plant above would still refuse - for the wrong reason - and the gate would start
  # firing on the prose that explains it.
  tolerate () { # tolerate <label> <shell that edits $R>
    local label=$1 edit=$2 R
    R="$BASE/$label"
    rm -rf "$R" && cp -r "$BASE/pristine" "$R"
    ( cd "$R" && eval "$edit" ) || { echo "  BAD       $label: the plant itself failed"; fail=$((fail+1)); return; }
    out=$(run_checks "$R"); rc=$?
    if [ $rc -eq 0 ]; then
      printf '  tolerated %-34s (the keyword was only in a comment)\n' "$label"; pass=$((pass+1))
    else
      printf '  BAD       %-34s refused on a COMMENT, so the stripper is not working:\n' "$label"
      grep -m2 '^REFUSED' <<<"$out" | sed 's/^/      /'; fail=$((fail+1))
    fi
  }

  tolerate a-comment-about-unsigned \
    'printf "\n(* why signedness= Unsigned never happens: of_z and null write Signed *)\n" >> infer/src/IR/IntLit.ml'
  tolerate a-comment-about-the-tag-elsewhere \
    'printf "(* nothing here builds IntLit.Unsigned *)\nlet n = 0\n" > infer/src/IR/Nearby.ml'

  plant add-an-unsigned-construction unsigned-sites \
    'printf "\nlet evil = {signedness= Unsigned; i= Z.zero; pointerness= NotPointer}\n" >> infer/src/IR/IntLit.ml'
  plant add-a-signed-construction construction-sites \
    'printf "\nlet extra = {signedness= Signed; i= Z.one; pointerness= Pointer}\n" >> infer/src/IR/IntLit.ml'
  plant delete-a-pinned-unsigned-site unsigned-sites \
    'grep -v "| true, Unsigned ->" infer/src/IR/IntLit.ml > t && mv t infer/src/IR/IntLit.ml'
  plant expose-the-tag-in-the-interface interface-is-abstract \
    'printf "\nval signedness_of : t -> int\n" >> infer/src/IR/IntLit.mli'
  plant make-the-type-concrete interface-is-abstract \
    'printf "\ntype t = {i: Z.t}\n" >> infer/src/IR/IntLit.mli'
  plant call-the-sexp-entry sexp-entry-has-no-caller \
    'printf "let revive s = IntLit.t_of_sexp s\n" > infer/src/IR/Reviver.ml'
  plant add-a-sexp-deriving-carrier sexp-entry-has-no-caller \
    'printf "type carrier = {v: IntLit.t} [@@deriving sexp]\n" > infer/src/IR/Carrier.ml'
  plant name-the-tag-elsewhere no-other-module-names-the-tag \
    'printf "let odd = IntLit.Unsigned\n" > infer/src/IR/Elsewhere.ml'

  echo "  self-test: $pass passed, $fail failed"
  [ $fail -eq 0 ] || return 1
  return 0
}

# ----------------------------------------------------------------------------- main
if [ "${1:-}" = --self-test ]; then
  self_test
  exit $?
fi
if [ $# -gt 0 ]; then echo "usage: $0 [--self-test]"; exit 2; fi

ROOT=${CITV_INTLIT_ROOT:-}
if [ -z "$ROOT" ]; then
  ROOT=$(git rev-parse --show-toplevel 2>/dev/null) || { echo "not in a git tree"; exit 2; }
fi
echo "reading $ROOT/infer/src/IR/IntLit.ml{,i}"
run_checks "$ROOT"; rc=$?
case $rc in
  0) echo "intlit premise: no negative unsigned literal can be created"; exit 0;;
  2) exit 2;;
  *) echo "intlit premise: REFUSED"; exit 1;;
esac
