/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "dedup_always.h"

namespace dedup_always {

std::mutex alpha;
std::mutex beta;

void split_alpha_then_beta_bad() {
  std::lock_guard<std::mutex> a(alpha);
  std::lock_guard<std::mutex> b(beta);
}

} // namespace dedup_always
