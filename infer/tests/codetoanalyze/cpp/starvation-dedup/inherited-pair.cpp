/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "inherited.h"

namespace inherited {

// The control: the same two sides, in ONE file. Here they are peers, so the
// inversion is reported between them and the forwarders in the service file
// must stay quiet.
void joined_gamma_then_delta_bad() {
  std::lock_guard<std::mutex> g(gamma);
  std::lock_guard<std::mutex> d(delta);
}

void joined_delta_then_gamma_bad() {
  std::lock_guard<std::mutex> d(delta);
  std::lock_guard<std::mutex> g(gamma);
}

} // namespace inherited
