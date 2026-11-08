/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "inherited.h"

namespace inherited {

// Forwarders take no lock of their own, so each inherits its whole critical
// pair from one public callee. Whether they report depends only on whether
// those callees are put together elsewhere.
class SplitService {
 public:
  void forward_bad() { split_alpha_then_beta_bad(); }
  void reverse_bad() { split_beta_then_alpha_bad(); }
};

class JoinedService {
 public:
  void forward_ok() { joined_gamma_then_delta_bad(); }
  void reverse_ok() { joined_delta_then_gamma_bad(); }
};

void drive(SplitService* split, JoinedService* joined) {
  split->forward_bad();
  split->reverse_bad();
  joined->forward_ok();
  joined->reverse_ok();
}

} // namespace inherited
