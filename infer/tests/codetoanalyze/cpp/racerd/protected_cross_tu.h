/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mutex>

namespace protected_cross_tu {

class Base {
 public:
  void set(int x);

 protected:
  void set_locked_bad(int x);

 private:
  std::mutex mutex_;
  int x_ = 0;
  int last_ = 0;
};

class SafeBase {
 public:
  void set(int x);

 protected:
  void set_locked_ok(int x);
  std::mutex mutex_;

 private:
  int x_ = 0;
  int last_ = 0;
};

} // namespace protected_cross_tu
