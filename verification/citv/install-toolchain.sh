#!/bin/bash
# Build the FV toolchain at a stable, documented location.
#
#   ./install-toolchain.sh          build whatever is missing under $CITV_HOME
#   ./install-toolchain.sh --check  report what is present, build nothing, exit 0/1
#
# WHY THIS EXISTS.  The bounded oracle (verification/citv/run.sh) and the Rocq proofs
# (verification/citv/rocq/run.sh) need an opam root, two switches, a Rocq install, a
# translator binary and -- on a host whose gmp has no headers -- a user-local GMP.  None
# of that is on Infer's build path and none of it is wanted by anything else, so it is
# not vendored and `make` never builds it.  For a while it lived in whichever scratch
# directory it was first built in, which exists on exactly one host; everywhere else the
# standing check said "toolchain not found", and that is exit 2, a check that could not
# be carried out.  The home is now $CITV_HOME, default ~/.citv-rocq, and rocq/setup.sh is
# still the single place that knows the layout.
#
# WHAT IT NEVER TOUCHES.  ~/.opam, Infer's switch, and any default/global switch: every
# opam call below names its switch.  rocq/setup.sh refuses outright if CITV_OPAMROOT is
# the default root.
#
# RESUMABLE AND IDEMPOTENT.  Every step tests for its own artefact and skips it, so a run
# that dies half way through rocq-core can simply be run again.  Nothing is removed.
#
# COST.  About an hour on an idle box and roughly 2.5 GB, almost all of it rocq-core and
# the two compilers.  CITV_SEED_ROOT=<an existing opam root> copies that root's
# download-cache in first, which turns most of the downloading into a file copy; the
# cache is content-addressed tarballs, so seeding cannot change what gets installed.
set -u

cd "$(dirname "$0")" || exit 2
HERE=$(pwd)

# setup.sh owns the paths and the pins.  Sourced with the escape hatch set, because the
# opam it checks for is one of the things this script installs.
CITV_SETUP_ALLOW_NO_OPAM=1 . ./rocq/setup.sh || exit 2

: "${CITV_JOBS:=8}"
: "${CITV_OPAM_VERSION:=2.3.0}"
: "${CITV_GMP_VERSION:=6.3.0}"
# sha256 of the official release assets, as fetched on the host this script was written
# on.  A mismatch stops the install rather than building something unidentified.
CITV_OPAM_SHA256=324e78e3f33efeba279aacf9f9610cfec7b2df7d7e0e1640f75f09de85f96cc9
CITV_GMP_SHA256=a3c2b80201b89e68616f4ad30bc66aee4927c3ce50e33929ca819d5c43538898

# Packages.  The Rocq switch carries the proofs' dependencies AND the translator's, since
# the translator is built in it.  The oracle switch carries exactly what verification/citv's
# dune files ask for -- no why3, no alt-ergo: an earlier switch had both because it was
# also used for a Cameleer spike, and they are not dependencies of anything here.
ROCQ_PKGS="rocq-core.9.0.1 rocq-stdlib.9.0.0 zarith.1.14 dune
           merlin-lib smart-print angstrom yojson csexp result ppxlib
           ppx_compare ppx_enumerate ppx_yojson_conv"
ORACLE_PKGS="dune zarith.1.14 yojson ppx_compare ppx_enumerate ppx_yojson_conv"

CHECK_ONLY=0
case "${1:-}" in
  --check) CHECK_ONLY=1 ;;
  "") ;;
  *) echo "usage: $0 [--check]"; exit 2 ;;
esac

say()  { printf '== %s\n' "$*"; }
skip() { printf '   already there: %s\n' "$*"; }
die()  { printf 'install-toolchain: %s\n' "$*" >&2; exit 1; }

TRANSLATOR_SRC=$CITV_HOME/src/rocq-of-ocaml

# --------------------------------------------------------------------------- --check
# Reports the same five artefacts the build produces, in the same order, so "what is
# missing" and "what will be built" cannot drift apart.
if [ $CHECK_ONLY = 1 ]; then
  rc=0
  # The two binaries are tested with -x, not -e. A present but non-executable translator
  # is a state the standing check refuses on, so a --check that called it "ok" would send
  # a reader looking somewhere else entirely.
  report() { if [ "$1" "$3" ]; then printf '  ok      %-22s %s\n' "$2" "$3"
             else printf '  MISSING %-22s %s\n' "$2" "$3"; rc=1; fi; }
  echo "CITV_HOME=$CITV_HOME"
  report -x opam            "$CITV_HOME/bin/opam"
  report -d "opam root"     "$CITV_OPAMROOT"
  report -d "rocq switch"   "$CITV_OPAMROOT/$CITV_SWITCH"
  report -d "oracle switch" "$CITV_OPAMROOT/$CITV_ORACLE_SWITCH"
  report -x translator      "${CITV_TRANSLATOR:-<empty>}"
  if command -v opam >/dev/null 2>&1 && [ -d "$CITV_OPAMROOT/$CITV_SWITCH" ]; then
    citv_assert_pins && echo "  ok      pins                   $CITV_PINS" || rc=1
  fi
  [ $rc = 0 ] && echo "TOOLCHAIN COMPLETE" || echo "TOOLCHAIN INCOMPLETE: run $0"
  exit $rc
fi

mkdir -p "$CITV_HOME/bin" "$CITV_HOME/src" "$CITV_HOME/dl" || die "cannot create $CITV_HOME"

# fetch DEST SHA256 URL...  Tries each URL in turn and keeps the first whose contents
# hash correctly.  Two mirrors rather than one because gmplib.org drops HTTP/2 streams
# often enough to have killed the first run of this script; --http1.1 is for the same
# reason.  The hash is checked before the file is put in place, so a truncated download
# is never mistaken for a cached one on the next run.
fetch() {
  local dest=$1 want=$2 url got; shift 2
  if [ -f "$dest" ] && [ "$(sha256sum "$dest" | awk '{print $1}')" = "$want" ]; then return 0; fi
  for url in "$@"; do
    echo "   fetching $url"
    curl -fsSL --http1.1 --retry 3 --retry-delay 5 -o "$dest.part" "$url" || continue
    got=$(sha256sum "$dest.part" | awk '{print $1}')
    if [ "$got" = "$want" ]; then mv "$dest.part" "$dest"; return 0; fi
    echo "   sha256 mismatch from $url: got $got, want $want"
  done
  rm -f "$dest.part"
  die "could not fetch $dest from any mirror"
}

# ------------------------------------------------------------------- 1. opam itself
# Into the home rather than relying on the host's, so that the home is the whole story
# and a fresh shell needs no PATH of its own.  setup.sh prepends $CITV_HOME/bin.
say "[1/8] opam $CITV_OPAM_VERSION"
if [ -x "$CITV_HOME/bin/opam" ]; then skip "$CITV_HOME/bin/opam"
else
  fetch "$CITV_HOME/dl/opam" "$CITV_OPAM_SHA256" \
        "https://github.com/ocaml/opam/releases/download/$CITV_OPAM_VERSION/opam-$CITV_OPAM_VERSION-x86_64-linux"
  cp "$CITV_HOME/dl/opam" "$CITV_HOME/bin/opam" && chmod +x "$CITV_HOME/bin/opam"
fi
PATH="$CITV_HOME/bin:$PATH"; export PATH
opam --version || die "the opam binary does not run"

# ------------------------------------------------------------------------- 2. GMP
# zarith needs gmp.h and a libgmp.so to link against.  A host with a gmp development
# package needs none of this; this box has libgmp.so.10 but no headers, so GMP is built
# user-locally under the home.  setup.sh puts it on the four paths a build needs.
say "[2/8] GMP"
if [ -e "$CITV_HOME/prefix/lib/libgmp.so" ]; then skip "$CITV_HOME/prefix"
elif [ -e /usr/include/gmp.h ] && { [ -e /usr/lib64/libgmp.so ] || [ -e /usr/lib/x86_64-linux-gnu/libgmp.so ]; }; then
  echo "   the host provides gmp headers and a link library; nothing to build"
else
  # CITV_GMP_TARBALL lets a host that already has the tarball skip the network entirely;
  # it is hashed exactly like a downloaded one, so it is a shortcut, not a trust decision.
  if [ -n "${CITV_GMP_TARBALL:-}" ]; then
    cp "$CITV_GMP_TARBALL" "$CITV_HOME/dl/gmp-$CITV_GMP_VERSION.tar.xz" || die "cannot read $CITV_GMP_TARBALL"
  fi
  fetch "$CITV_HOME/dl/gmp-$CITV_GMP_VERSION.tar.xz" "$CITV_GMP_SHA256" \
        "https://ftp.gnu.org/gnu/gmp/gmp-$CITV_GMP_VERSION.tar.xz" \
        "https://gmplib.org/download/gmp/gmp-$CITV_GMP_VERSION.tar.xz"
  rm -rf "$CITV_HOME/src/gmp-$CITV_GMP_VERSION"
  tar -C "$CITV_HOME/src" -xf "$CITV_HOME/dl/gmp-$CITV_GMP_VERSION.tar.xz" || die "cannot unpack gmp"
  ( cd "$CITV_HOME/src/gmp-$CITV_GMP_VERSION" &&
    ./configure --prefix="$CITV_HOME/prefix" --enable-shared --disable-static &&
    make -j"$CITV_JOBS" && make install ) > "$CITV_HOME/gmp-build.log" 2>&1 ||
    { tail -20 "$CITV_HOME/gmp-build.log"; die "the GMP build failed, see $CITV_HOME/gmp-build.log"; }
  # setup.sh only adds these when the directory exists, which it did not when this script
  # sourced it, so add them for the rest of this run.
  export LD_LIBRARY_PATH="$CITV_HOME/prefix/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
  export LIBRARY_PATH="$CITV_HOME/prefix/lib${LIBRARY_PATH:+:$LIBRARY_PATH}"
  export C_INCLUDE_PATH="$CITV_HOME/prefix/include${C_INCLUDE_PATH:+:$C_INCLUDE_PATH}"
  export PKG_CONFIG_PATH="$CITV_HOME/prefix/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
fi

# ------------------------------------------------------------------- 3. the opam root
say "[3/8] opam root at $CITV_OPAMROOT"
if [ -d "$CITV_OPAMROOT/repo" ]; then skip "$CITV_OPAMROOT"
else
  if [ -n "${CITV_SEED_ROOT:-}" ] && [ -d "$CITV_SEED_ROOT/download-cache" ]; then
    say "      seeding download-cache from $CITV_SEED_ROOT (read-only on the source)"
    mkdir -p "$CITV_OPAMROOT"
    cp -a "$CITV_SEED_ROOT/download-cache" "$CITV_OPAMROOT/" || die "seeding failed"
  fi
  # --bare: no switch, and in particular no default switch.  --no-setup: never edit a
  # shell profile.  --disable-sandboxing: bwrap is not available unprivileged here.
  opam init --root "$CITV_OPAMROOT" --bare --no-setup --disable-sandboxing -y ||
    die "opam init failed"
fi
opam update --root "$CITV_OPAMROOT" -y >/dev/null 2>&1 || true

export OPAMYES=1

# --no-depexts is not a workaround here, it is the correct answer. zarith declares the
# system package gmp-devel, and opam's only way to satisfy that is to run dnf as root.
# Step 2 has already provided exactly what gmp-devel provides -- gmp.h and a libgmp.so --
# under $CITV_HOME/prefix, and setup.sh puts it on C_INCLUDE_PATH, LIBRARY_PATH,
# LD_LIBRARY_PATH and PKG_CONFIG_PATH. Without the flag opam stops at an interactive
# "1/2/3/4" prompt and the install fails; with it, zarith builds against the local GMP and
# the pin assertion at the end is what proves it really did.
make_switch() {  # make_switch NAME COMPILER PKGS...
  local name=$1 compiler=$2; shift 2
  if [ -d "$CITV_OPAMROOT/$name" ]; then skip "switch $name"
  else
    opam switch create "$name" "$compiler" --no-install --no-depexts -j "$CITV_JOBS" ||
      die "could not create switch $name on $compiler"
  fi
  opam install --switch="$name" --no-depexts -j "$CITV_JOBS" $* ||
    die "could not install into switch $name: $*"
}

# ----------------------------------------------------------------- 4. the Rocq switch
# The versions are asserted at the end, not merely requested: opam left to itself resolves
# rocq-core to 9.2/9.3, outside rocq-of-ocaml's >= 9.0 & < 9.1 window.
say "[4/8] switch $CITV_SWITCH ($CITV_ROCQ_COMPILER) and the Rocq packages"
make_switch "$CITV_SWITCH" "$CITV_ROCQ_COMPILER" $ROCQ_PKGS

# --------------------------------------------------------------- 5. the oracle switch
say "[5/8] switch $CITV_ORACLE_SWITCH ($CITV_ORACLE_COMPILER) and the oracle packages"
make_switch "$CITV_ORACLE_SWITCH" "$CITV_ORACLE_COMPILER" $ORACLE_PKGS

# ------------------------------------------------------- 6. the translator's source
say "[6/8] rocq-of-ocaml at $CITV_TRANSLATOR_COMMIT"
if [ -d "$TRANSLATOR_SRC/.git" ]; then skip "$TRANSLATOR_SRC"
else
  git clone "$CITV_TRANSLATOR_REPO" "$TRANSLATOR_SRC" || die "clone failed"
fi
git -C "$TRANSLATOR_SRC" checkout -q "$CITV_TRANSLATOR_COMMIT" ||
  die "cannot check out $CITV_TRANSLATOR_COMMIT in $TRANSLATOR_SRC"
got=$(git -C "$TRANSLATOR_SRC" rev-parse HEAD)
case "$got" in "$CITV_TRANSLATOR_COMMIT"*) ;; *) die "HEAD is $got, not $CITV_TRANSLATOR_COMMIT" ;; esac

# ------------------------------------------------------- 7. the translator executable
# Only the executable, not the opam package: the package's build also compiles the Rocq
# library in proofs/, which step 8 does separately so that a failure there is legible.
# src/dune shows the executable itself needs merlin-lib, smart_print, angstrom and yojson.
say "[7/8] rocqOfOCaml.exe"
if [ -x "$CITV_TRANSLATOR" ]; then skip "$CITV_TRANSLATOR"
else
  ( cd "$TRANSLATOR_SRC" && opam exec --switch="$CITV_SWITCH" -- dune build src/rocqOfOCaml.exe ) ||
    die "the translator build failed"
  cp "$TRANSLATOR_SRC/_build/default/src/rocqOfOCaml.exe" "$CITV_TRANSLATOR" ||
    die "the translator was built but could not be copied to $CITV_TRANSLATOR"
fi

# ------------------------------------------------- 8. the RocqOfOCaml support library
# Every generated .v starts with "Require Import RocqOfOCaml.RocqOfOCaml", so without this
# nothing under rocq/v compiles.  It is the library whose `assert` parameter is flatly
# inconsistent; check-axioms.sh whitelists nothing because of it.
say "[8/8] the RocqOfOCaml support library"
SWPREFIX=$CITV_OPAMROOT/$CITV_SWITCH
if [ -d "$SWPREFIX/lib/coq/user-contrib/RocqOfOCaml" ] ||
   [ -d "$SWPREFIX/lib/rocq/user-contrib/RocqOfOCaml" ]; then
  skip "RocqOfOCaml is installed in $CITV_SWITCH"
else
  ( cd "$TRANSLATOR_SRC/proofs" &&
    opam exec --switch="$CITV_SWITCH" -- sh -c './configure.sh' &&
    opam exec --switch="$CITV_SWITCH" -- make -j"$CITV_JOBS" &&
    opam exec --switch="$CITV_SWITCH" -- make install ) > "$CITV_HOME/rocqofocaml-lib.log" 2>&1 ||
    { tail -20 "$CITV_HOME/rocqofocaml-lib.log"
      die "the support library failed to build, see $CITV_HOME/rocqofocaml-lib.log"; }
fi

# ------------------------------------------------------------------------- verify
say "verifying"
citv_assert_pins || die "the toolchain was built but does not match the pins"
"$CITV_TRANSLATOR" -help >/dev/null 2>&1 || "$CITV_TRANSLATOR" --help >/dev/null 2>&1 ||
  echo "   note: the translator does not answer --help; it is exercised by rocq/run.sh"
echo
echo "TOOLCHAIN INSTALLED at $CITV_HOME"
echo "  pins      $CITV_PINS, translator $CITV_TRANSLATOR_COMMIT"
echo "  next      cd $HERE/rocq && ./run.sh"
