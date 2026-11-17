#!/bin/bash
# Pins, and the one place that knows where the toolchain lives.  Sourced, not executed.
#
# This NEVER touches Infer's own opam switch or build.  Infer does not depend on Rocq and
# never asks for it; this directory is outside the dune root, so make and dune do not see
# it.
#
# WHERE THE TOOLCHAIN LIVES.  One variable, CITV_HOME, and everything else hangs off it.
# It defaults to ~/.citv-rocq rather than to a directory belonging to whoever built it
# first: a path under somebody's scratch tree exists on exactly one host, so the standing
# check reported "toolchain not found" everywhere else -- and that is exit 2, a check that
# could not be carried out, not a pass.  Build the home with install-toolchain.sh one
# directory up.  Every variable below is overridable, so a second install (a different
# Rocq, a bisect, a test stand-in) is a CITV_HOME away and needs no edit here.
: "${CITV_HOME:=$HOME/.citv-rocq}"
: "${CITV_OPAMROOT:=$CITV_HOME/opamroot}"
: "${CITV_SWITCH:=coq541}"
: "${CITV_ORACLE_SWITCH:=fv}"
# No colon on the translator alone: ':=' would default an EMPTY value too, and an empty
# CITV_TRANSLATOR is how drift.sh is told to run deliberately without one. '=' defaults
# only when it is unset.
: "${CITV_TRANSLATOR=$CITV_HOME/bin/rocqOfOCaml.exe}"
# Where the Z3 python bindings live. Exported as its own variable and deliberately NOT
# folded into PYTHONPATH: PYTHONPATH is inherited by every python a sourcing shell runs,
# including Infer's own build scripts, and putting a 67 MB site-packages tree in front of
# them to serve one oracle is a side effect nobody asked for. The arith runner sets
# PYTHONPATH from this for the one command that needs it. The z3 BINARY needs nothing
# here: it is linked into $CITV_HOME/bin, which is already prepended to PATH below.
: "${CITV_Z3_PYTHONPATH=$CITV_HOME/python}"

# Exact pins.  opam resolves rocq-core to 9.3.0/9.2.0 by default, which is OUTSIDE
# rocq-of-ocaml's window (>= 9.0 & < 9.1), so the versions are asserted below rather than
# merely requested.
CITV_PINS="ocaml:5.4.1 rocq-core:9.0.1 rocq-stdlib:9.0.0 zarith:1.14"
CITV_ROCQ_COMPILER=ocaml-base-compiler.5.4.1
CITV_ORACLE_COMPILER=ocaml-base-compiler.5.2.0
CITV_TRANSLATOR_COMMIT=1ac69a72
CITV_TRANSLATOR_REPO=https://github.com/formal-land/coq-of-ocaml.git

case "$CITV_OPAMROOT" in
  "$HOME/.opam"|"$HOME/.opam/") echo "refusing to use the default opam root"; return 2 2>/dev/null || exit 2 ;;
esac

# The home is self-contained on purpose: its own opam binary and, on a host with no gmp
# headers, its own GMP.  Prepending them here is what lets a fresh shell with no agent
# environment run the check at all.  Both are skipped when absent, so a host that has
# opam and gmp from its package manager is unaffected.
#
# DO NOT SOURCE THIS FILE INTO A SHELL YOU THEN BUILD INFER IN.  The four variables below
# are four of the inputs the build harness hashes into its context digest (PATH,
# LD_LIBRARY_PATH, LIBRARY_PATH, C_INCLUDE_PATH), so a shell carrying them computes a
# different digest, misses every cached build, and turns a gate comparison into a fresh
# baseline.  Every caller here -- run.sh, translate.sh, drift.sh, install-toolchain.sh --
# is a subprocess, so nothing escapes into the parent; the hazard is a human sourcing it
# by hand, which the "Sourced, not executed" line above otherwise invites.
[ -d "$CITV_HOME/bin" ] && PATH="$CITV_HOME/bin:$PATH"
[ -d "$CITV_HOME/prefix/lib" ] && {
  LD_LIBRARY_PATH="$CITV_HOME/prefix/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
  LIBRARY_PATH="$CITV_HOME/prefix/lib${LIBRARY_PATH:+:$LIBRARY_PATH}"
  C_INCLUDE_PATH="$CITV_HOME/prefix/include${C_INCLUDE_PATH:+:$C_INCLUDE_PATH}"
  PKG_CONFIG_PATH="$CITV_HOME/prefix/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
  export LD_LIBRARY_PATH LIBRARY_PATH C_INCLUDE_PATH PKG_CONFIG_PATH
}
export PATH CITV_HOME CITV_OPAMROOT CITV_TRANSLATOR CITV_Z3_PYTHONPATH
export OPAMROOT="$CITV_OPAMROOT"
export ROCQ_RUN="opam exec --switch=$CITV_SWITCH --"

# No opam means the runners cannot run.  Say so here, in the toolchain's own words, rather
# than letting it surface as an empty version string and a PIN MISMATCH: citv_check.sh
# routes '^REFUSED: toolchain' to "could not be carried out", and a pin mismatch is a
# different and much more alarming fact.  install-toolchain.sh sets the escape hatch,
# because it sources this file in order to build the opam it is being told is missing.
if [ "${CITV_SETUP_ALLOW_NO_OPAM:-0}" != 1 ] && ! command -v opam >/dev/null 2>&1; then
  echo "REFUSED: toolchain -- no opam on PATH, and none at $CITV_HOME/bin/opam."
  echo "         Build the toolchain with verification/citv/install-toolchain.sh"
  echo "         (override the location with CITV_HOME)."
  return 2 2>/dev/null || exit 2
fi

# IS THE TRANSLATOR USABLE?  -x alone is not the test, because a DIRECTORY passes it: every
# directory you can search is "executable".  So CITV_TRANSLATOR=/some/dir got all the way to
# translate.sh, failed to run with a shell error, and had that error text diffed against the
# committed error pin -- reported as "the translator's error set changed", a missing toolchain
# wearing a soundness verdict.  -f as well as -x is the whole fix.  One predicate, used by
# every caller, so the three guards cannot drift apart again.
citv_translator_ok() {
  [ -n "${CITV_TRANSLATOR:-}" ] && [ -f "$CITV_TRANSLATOR" ] && [ -x "$CITV_TRANSLATOR" ]
}

citv_assert_pins() {
  local bad=0 p name want got
  for p in $CITV_PINS; do
    name=${p%%:*}; want=${p##*:}
    if [ "$name" = ocaml ]; then
      got=$($ROCQ_RUN ocamlc -version 2>/dev/null)
    else
      got=$(opam list --switch="$CITV_SWITCH" --installed --short --columns=version "$name" 2>/dev/null | head -1)
    fi
    if [ "$got" != "$want" ]; then
      echo "PIN MISMATCH: $name is '$got', must be '$want'"; bad=1
    fi
  done
  return $bad
}
