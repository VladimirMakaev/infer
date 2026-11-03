/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mutex>

// a guard moved into a callee takes the lock with it, and releases it when the
// callee returns, so the caller holds nothing afterwards
namespace guard_by_value {

using Lock = std::unique_lock<std::mutex>;

// releases its caller's guard for its own lifetime
template <typename L>
class ReverseLock {
 public:
  explicit ReverseLock(L& l) : l_(l) { l_.unlock(); }
  ~ReverseLock() { l_.lock(); }

 private:
  L& l_;
};

class Moved {
 public:
  void release_in_callee_ok() {
    Lock l(mutex_);
    release(std::move(l));
    std::lock_guard<std::mutex> g(mutex_);
  }

  // the callee relocks, but its own guard releases again when it returns
  void relock_in_callee_ok() {
    Lock l(mutex_);
    release_and_take(std::move(l));
    std::lock_guard<std::mutex> g(mutex_);
  }

  // the callee locks a deferred guard, which releases when it returns
  void take_in_callee_ok() {
    Lock l(mutex_, std::defer_lock);
    take(std::move(l));
    std::lock_guard<std::mutex> g(mutex_);
  }

  // the callee takes the mutex its own parameter still holds
  void take_again_in_callee_bad() {
    Lock l(mutex_);
    take_again(std::move(l));
  }

 private:
  void release(Lock l) { l.unlock(); }

  void release_and_take(Lock l) {
    l.unlock();
    l.lock();
  }

  void take(Lock l) { l.lock(); }

  void take_again(Lock l) { std::lock_guard<std::mutex> g(mutex_); }

  std::mutex mutex_;
};

class ReverseLocked {
 public:
  // the callee drops the lock for the whole scope of the reverse lock.
  // Still reported: the guard escapes into the reverse lock, so the release is
  // not applied to the caller. Moving the guard by value is not enough on its
  // own; see the reverse lock limitation of the guard-by-reference tests.
  void FP_lock_in_scope_ok() {
    Lock l(mutex_);
    unlocked(std::move(l));
  }

  // the reverse lock ends before the second acquisition, which deadlocks
  void lock_after_scope_bad() {
    Lock l(mutex_);
    unlocked_then_take(std::move(l));
  }

 private:
  void take() { std::lock_guard<std::mutex> g(mutex_); }

  void unlocked(Lock l) {
    ReverseLock<Lock> unlocked(l);
    take();
  }

  void unlocked_then_take(Lock l) {
    { ReverseLock<Lock> unlocked(l); }
    take();
  }

  std::mutex mutex_;
};

class Transfer {
 public:
  // a moved-from guard holds nothing, so moving it again is harmless
  void move_deferred_twice_ok() {
    Lock a(mutex_, std::defer_lock);
    Lock b(std::move(a));
    Lock c(std::move(a));
  }

  // the lock follows the move and is still held at the second acquisition
  void move_held_bad() {
    Lock a(mutex_);
    Lock b(std::move(a));
    std::lock_guard<std::mutex> g(mutex_);
  }

 private:
  std::mutex mutex_;
};

} // namespace guard_by_value
