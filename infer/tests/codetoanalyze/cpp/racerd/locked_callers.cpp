/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mutex>

// A helper may only be credited with its callers' lock when the callers really hold it. Two
// helpers that call only each other have no locked caller outside the cycle, so neither may be
// credited on the strength of the other. This is already the intended behaviour; it is here so
// that a change to how callers are enumerated cannot quietly lose it.
namespace locked_callers {

class Cycle {
 public:
  void tick_ok() {
    std::lock_guard<std::mutex> lock(mutex_);
    x_ = x_ + 1;
  }

 protected:
  void ping_bad(int n) {
    x_ = x_ + 1;
    if (n > 0) {
      pong_bad(n - 1);
    }
  }

  void pong_bad(int n) {
    x_ = x_ + 1;
    if (n > 0) {
      ping_bad(n - 1);
    }
  }

 private:
  std::mutex mutex_;
  int x_ = 0;
};

} // namespace locked_callers
