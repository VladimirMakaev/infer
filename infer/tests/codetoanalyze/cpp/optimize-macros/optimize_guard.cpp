/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Built with -O2. A source is entitled to assert the optimisation level it was
// compiled with; capture must not take that assertion away from it.
#if !defined(__OPTIMIZE__)
#error "capture dropped __OPTIMIZE__ from an optimised build"
#endif

// -O2 defines __OPTIMIZE__ and not __OPTIMIZE_SIZE__, so restoring the level's
// macros has to be exact rather than blanket.
#if defined(__OPTIMIZE_SIZE__)
#error "capture added __OPTIMIZE_SIZE__ to a build that did not ask for size"
#endif

// -O0 is still what the compiler runs under, so __NO_INLINE__ must be gone with
// the rest of the unoptimised level's macros.
#if defined(__NO_INLINE__)
#error "capture left __NO_INLINE__ defined in an optimised build"
#endif

int optimized_build_is_still_analysed_bad() {
  int* p = nullptr;
  return *p;
}
