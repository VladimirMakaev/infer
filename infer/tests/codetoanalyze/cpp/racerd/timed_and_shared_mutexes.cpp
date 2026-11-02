/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// The timed and shared mutex types of the standard library and boost. The
// [folly::Synchronized] case is in synchronized_null_mutex.cpp, which already
// has the stub.

#include <chrono>
#include <mutex>
#include <shared_mutex>

namespace boost {
class timed_mutex {
 public:
  void lock() {}
  void unlock() {}
  bool try_lock() { return true; }
  bool try_lock_for(int) { return true; }
};
class upgrade_mutex {
 public:
  void lock() {}
  void unlock() {}
  void lock_shared() {}
  void unlock_shared() {}
  void lock_upgrade() {}
  void unlock_upgrade() {}
};
} // namespace boost

namespace timed_and_shared_mutexes {

struct Data {
  int value;
};

// Locking the mutex directly, without a guard.
class SharedTimedDirect {
 public:
  void write_ok(int v) {
    mutex_.lock();
    value_ = v;
    mutex_.unlock();
  }
  int read_shared_ok() {
    mutex_.lock_shared();
    int value = value_;
    mutex_.unlock_shared();
    return value;
  }
  int read_unlocked_bad() { return value_; }

 private:
  std::shared_timed_mutex mutex_;
  int value_ = 0;
};

// A bounded try-lock is how a timed mutex is usually taken.
class TimedTryLock {
 public:
  void write_ok(int v) {
    if (mutex_.try_lock_for(std::chrono::seconds(1))) {
      value_ = v;
      mutex_.unlock();
    }
  }
  int read_unlocked_bad() { return value_; }

 private:
  std::timed_mutex mutex_;
  int value_ = 0;
};

class BoostTimed {
 public:
  void write_ok(int v) {
    mutex_.lock();
    value_ = v;
    mutex_.unlock();
  }
  int read_unlocked_bad() { return value_; }

 private:
  boost::timed_mutex mutex_;
  int value_ = 0;
};

class BoostUpgrade {
 public:
  void write_ok(int v) {
    mutex_.lock();
    value_ = v;
    mutex_.unlock();
  }
  int read_upgrade_ok() {
    mutex_.lock_upgrade();
    int value = value_;
    mutex_.unlock_upgrade();
    return value;
  }
  int read_unlocked_bad() { return value_; }

 private:
  boost::upgrade_mutex mutex_;
  int value_ = 0;
};
} // namespace timed_and_shared_mutexes
