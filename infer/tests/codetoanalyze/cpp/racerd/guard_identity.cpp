/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mutex>

namespace guard_identity {

// A reference-guard positive control is in wrapper_lock_use.cpp. These shapes must not
// establish a concurrent context merely because constructor/destructor counts complement.
class InternalGuard {
  std::mutex internal_;

 public:
  explicit InternalGuard(std::mutex&) { internal_.lock(); }
  ~InternalGuard() { internal_.unlock(); }
};

class InternalMutexOwner {
  std::mutex mutex_;
  int value_{0};

 public:
  void write() {
    InternalGuard guard(mutex_);
    ++value_;
  }
  // Known false evidence shape: the guard locks its own storage, not mutex_.
  int FP_internal_guard_read_ok() { return value_; }
};

class WrongReleaseGuard {
  std::mutex& release_target_;

 public:
  WrongReleaseGuard(std::mutex& acquire_target, std::mutex& release_target)
      : release_target_(release_target) {
    acquire_target.lock();
  }
  ~WrongReleaseGuard() { release_target_.unlock(); }
};

class WrongReleaseOwner {
  std::mutex mutex_;
  std::mutex other_;
  int value_{0};

 public:
  void write() {
    WrongReleaseGuard guard(mutex_, other_);
    ++value_;
  }
  // Known false evidence shape: one mutex is acquired and a different one released.
  int FP_wrong_release_read_ok() { return value_; }
};

} // namespace guard_identity
