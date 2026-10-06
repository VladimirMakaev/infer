/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "protected_cross_tu.h"

namespace protected_cross_tu {

void Base::set(int x) {
  std::lock_guard<std::mutex> lock(mutex_);
  set_locked_bad(x);
}

void Base::set_locked_bad(int x) {
  last_ = x_;
  x_ = x;
}

void SafeBase::set(int x) {
  std::lock_guard<std::mutex> lock(mutex_);
  set_locked_ok(x);
}

void SafeBase::set_locked_ok(int x) {
  last_ = x_;
  x_ = x;
}

} // namespace protected_cross_tu
