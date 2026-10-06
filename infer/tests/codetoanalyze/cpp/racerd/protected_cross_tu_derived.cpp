/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "protected_cross_tu.h"

namespace protected_cross_tu {

class Derived : public Base {
 public:
  void set_unlocked_bad(int x) { set_locked_bad(x); }
};

class SafeDerived : public SafeBase {
 public:
  void set_guarded_ok(int x) {
    std::lock_guard<std::mutex> lock(mutex_);
    set_locked_ok(x);
  }
};

void use(Derived& unsafe, SafeDerived& safe) {
  unsafe.set_unlocked_bad(1);
  safe.set_guarded_ok(1);
}

} // namespace protected_cross_tu
