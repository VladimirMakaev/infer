/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "inherited.h"

namespace inherited {

std::mutex alpha;
std::mutex beta;
std::mutex gamma;
std::mutex delta;

// One side of the inversion, alone in its file. Public, so it passes the
// "reportable callee" test, but it never sees the other side: that lives in
// another file and the locks are globals, so there is no class to search.
void split_alpha_then_beta_bad() {
  std::lock_guard<std::mutex> a(alpha);
  std::lock_guard<std::mutex> b(beta);
}

} // namespace inherited
