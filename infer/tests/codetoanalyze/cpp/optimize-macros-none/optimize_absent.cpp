/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Built with -O0: the optimisation macros must be absent, exactly as they are
// for a build that passes no optimisation flag at all.
#if defined(__OPTIMIZE__)
#error "capture defined __OPTIMIZE__ for an unoptimised build"
#endif

#if defined(__OPTIMIZE_SIZE__)
#error "capture defined __OPTIMIZE_SIZE__ for an unoptimised build"
#endif

int unoptimized_build_is_analysed_bad() {
  int* p = nullptr;
  return *p;
}
