/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mutex>

namespace header_locks {

// not modelled as a lock: callers acquire [m_] through the inline bodies below
class Mutex {
 public:
  void lock() { m_.lock(); }
  void unlock() { m_.unlock(); }

 private:
  std::mutex m_;
};

// A second wrapper, so that a class using it has its own canonical lock: every lock taken
// through Mutex is the field Mutex::m_, which makes every inversion in one client file the
// same canonical pair and leaves all but the first unasserted.
class CalleeMutex {
 public:
  void lock() { m_.lock(); }
  void unlock() { m_.unlock(); }

 private:
  std::mutex m_;
};

} // namespace header_locks
