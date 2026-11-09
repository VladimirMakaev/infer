/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "dedup_always.h"

namespace dedup_always {

void split_beta_then_alpha_bad() {
  std::lock_guard<std::mutex> b(beta);
  std::lock_guard<std::mutex> a(alpha);
}

} // namespace dedup_always
