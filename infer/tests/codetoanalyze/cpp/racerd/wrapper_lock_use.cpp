/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mutex>

namespace wrapper_lock_use {

class MutexAdapter {
  std::mutex mutex_;

 public:
  void lock() { mutex_.lock(); }
  void unlock() { mutex_.unlock(); }
};

template <typename Mutex>
class AdapterGuard {
  Mutex& mutex_;

 public:
  explicit AdapterGuard(Mutex& mutex) : mutex_(mutex) { mutex_.lock(); }
  ~AdapterGuard() { mutex_.unlock(); }
};

class UserGuard {
  MutexAdapter& mutex_;

  static void acquire(MutexAdapter& mutex) { mutex.lock(); }

 public:
  explicit UserGuard(MutexAdapter& mutex) : mutex_(mutex) { acquire(mutex_); }
  ~UserGuard() { mutex_.unlock(); }
};

class StandardGuard {
  std::mutex mutex_;
  int value_{0};

 public:
  void write() {
    std::lock_guard<std::mutex> guard(mutex_);
    ++value_;
  }
  int read_bad() { return value_; }
  int read_ok() {
    std::lock_guard<std::mutex> guard(mutex_);
    return value_;
  }
};

class AdaptedGuard {
  MutexAdapter mutex_;
  int value_{0};

 public:
  void write() {
    AdapterGuard<MutexAdapter> guard(mutex_);
    ++value_;
  }
  int read_bad() { return value_; }
  int read_ok() {
    AdapterGuard<MutexAdapter> guard(mutex_);
    return value_;
  }
};

class DelegatedGuard {
  MutexAdapter mutex_;
  int value_{0};

 public:
  void write() {
    UserGuard guard(mutex_);
    ++value_;
  }
  int read_bad() { return value_; }
  int read_ok() {
    UserGuard guard(mutex_);
    return value_;
  }
};

class BalancedHelper {
  MutexAdapter mutex_;
  int value_{0};

  void protected_write() {
    AdapterGuard<MutexAdapter> guard(mutex_);
    ++value_;
  }
  int protected_read() {
    AdapterGuard<MutexAdapter> guard(mutex_);
    return value_;
  }

 public:
  void write() { protected_write(); }
  int read_bad() { return value_; }
  int read_ok() { return protected_read(); }
};

void owned_object_ok() {
  AdaptedGuard value;
  value.write();
  value.read_bad();
}

} // namespace wrapper_lock_use
