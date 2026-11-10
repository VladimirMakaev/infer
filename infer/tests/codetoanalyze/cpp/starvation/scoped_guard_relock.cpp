/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mutex>

namespace scoped_guard_relock {

class __attribute__((capability("mutex"))) Mutex {
 public:
  std::mutex native;
  void lock() __attribute__((acquire_capability())) { native.lock(); }
  void unlock() __attribute__((release_capability())) { native.unlock(); }
};

class __attribute__((scoped_lockable)) Guard {
 public:
  explicit Guard(Mutex& mutex) __attribute__((acquire_capability(mutex)))
      : lock_(mutex.native) {}
  ~Guard() __attribute__((release_capability())) = default;
  void lock() __attribute__((acquire_capability())) { lock_.lock(); }
  void unlock() __attribute__((release_capability())) { lock_.unlock(); }

 private:
  std::unique_lock<std::mutex> lock_;
};

class Session {
  Mutex mutex_;

 public:
  void take() { Guard guard(mutex_); }

  void scope_exit_ok() {
    { Guard guard(mutex_); }
    take();
  }

  void explicit_unlock_ok() {
    Guard guard(mutex_);
    guard.unlock();
    take();
  }

  void restore(Guard& guard, bool repeat) {
    guard.unlock();
    guard.lock();
    if (repeat) {
      guard.unlock();
      guard.lock();
    }
  }

  void conditional_restore_ok(bool repeat) {
    Guard guard(mutex_);
    restore(guard, repeat);
    guard.unlock();
    take();
  }

  void delegate_restore_ok(bool repeat) { conditional_restore_ok(repeat); }

  void guarded_restore_ok(bool repeat) {
    Guard guard(mutex_);
    if (repeat) {
      restore(guard, repeat);
    }
    guard.unlock();
    take();
  }

  void held_guard_bad() {
    Guard guard(mutex_);
    take();
  }

};

// A class each from here down. A deadlock is grouped per canonical lock and a lock is named
// by the field it is, so these further self deadlocks on Session::mutex_ were merged into
// held_guard_bad's row and stopped being asserted.
class RestoredSession {
  Mutex mutex_;

 public:
  void take() { Guard guard(mutex_); }

  void restore(Guard& guard, bool repeat) {
    guard.unlock();
    guard.lock();
    if (repeat) {
      guard.unlock();
      guard.lock();
    }
  }

  void restored_guard_bad(bool repeat) {
    Guard guard(mutex_);
    restore(guard, repeat);
    take();
  }
};

class RelockTwiceSession {
  Mutex mutex_;

 public:
  void relock_twice_bad() {
    Guard guard(mutex_);
    guard.unlock();
    guard.lock();
    mutex_.lock();
    mutex_.unlock();
  }
};

// A method's body must still contribute its critical pairs even when its net lock state is empty.
class __attribute__((scoped_lockable)) BodyGuard {
 public:
  explicit BodyGuard(Mutex& mutex, std::mutex& other)
      __attribute__((acquire_capability(mutex)))
      : lock_(mutex.native), other_(other) {}
  ~BodyGuard() __attribute__((release_capability())) = default;

  void unlock() __attribute__((release_capability())) {
    lock_.unlock();
    other_.lock();
    other_.lock();
    other_.unlock();
    other_.unlock();
  }

 private:
  std::unique_lock<std::mutex> lock_;
  std::mutex& other_;
};

void body_deadlock_bad(Mutex& mutex, std::mutex& other) {
  BodyGuard guard(mutex, other);
  guard.unlock();
}

} // namespace scoped_guard_relock
