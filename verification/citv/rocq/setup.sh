#!/bin/bash
# Pins, and the one place that knows where the toolchain lives.  Sourced, not executed.
#
# This NEVER touches Infer's own opam switch or build.  Infer does not depend on Rocq and
# never asks for it; this directory is outside the dune root, so make and dune do not see
# it.
#
# Exact pins.  opam resolves rocq-core to 9.3.0/9.2.0 by default, which is OUTSIDE
# rocq-of-ocaml's window (>= 9.0 & < 9.1), so the versions are asserted below rather than
# merely requested.
: "${CITV_OPAMROOT:=$HOME/.citv-rocq/opamroot}"
: "${CITV_SWITCH:=coq541}"
CITV_PINS="ocaml:5.4.1 rocq-core:9.0.1 rocq-stdlib:9.0.0 zarith:1.14"
CITV_TRANSLATOR_COMMIT=1ac69a72

case "$CITV_OPAMROOT" in
  "$HOME/.opam"|"$HOME/.opam/") echo "refusing to use the default opam root"; return 2 2>/dev/null || exit 2 ;;
esac

export OPAMROOT="$CITV_OPAMROOT"
export ROCQ_RUN="opam exec --switch=$CITV_SWITCH --"

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
