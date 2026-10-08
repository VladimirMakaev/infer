/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mutex>
#include <shared_mutex>

// a callee may lock or unlock a guard of its caller passed by reference
namespace guard_by_reference {

class SharedCache {
 public:
  int lookup_ok(bool hit) {
    std::shared_lock<std::shared_mutex> read(mutex_);
    if (hit) {
      return value_;
    }
    return miss(read);
  }

  int lookup_through_helper_ok(bool hit) {
    std::shared_lock<std::shared_mutex> read(mutex_);
    if (hit) {
      return value_;
    }
    return forward_miss(read);
  }

  int lookup_bad(bool hit) {
    std::shared_lock<std::shared_mutex> read(mutex_);
    if (hit) {
      return value_;
    }
    return miss_without_unlock(read);
  }

 private:
  int miss(std::shared_lock<std::shared_mutex>& read) {
    read.unlock();
    std::unique_lock<std::shared_mutex> write(mutex_);
    return ++value_;
  }

  int forward_miss(std::shared_lock<std::shared_mutex>& read) {
    return miss(read);
  }

  int miss_without_unlock(std::shared_lock<std::shared_mutex>& read) {
    std::unique_lock<std::shared_mutex> write(mutex_);
    return ++value_;
  }

  std::shared_mutex mutex_;
  int value_;
};

class Relock {
 public:
  void unlock_in_callee_ok() {
    std::unique_lock<std::mutex> l(mutex_);
    release(l);
    std::lock_guard<std::mutex> g(mutex_);
  }

  void relock_in_callee_bad() {
    std::unique_lock<std::mutex> l(mutex_);
    release_and_take(l);
    std::lock_guard<std::mutex> g(mutex_);
  }

  void lock_in_callee_bad() {
    std::unique_lock<std::mutex> l(mutex_, std::defer_lock);
    take(l);
    std::lock_guard<std::mutex> g(mutex_);
  }

 private:
  void release(std::unique_lock<std::mutex>& l) { l.unlock(); }

  void release_and_take(std::unique_lock<std::mutex>& l) {
    l.unlock();
    l.lock();
  }

  void take(std::unique_lock<std::mutex>& l) { l.lock(); }

  std::mutex mutex_;
};

class RelockUnderOtherLock {
 public:
  // retakes mutex_1 while holding mutex_2
  void relock_bad() {
    std::unique_lock<std::mutex> lock1(mutex_1);
    std::lock_guard<std::mutex> lock2(mutex_2);
    release_and_take(lock1);
  }

 private:
  void release_and_take(std::unique_lock<std::mutex>& l) {
    l.unlock();
    l.lock();
  }

  std::mutex mutex_1;
  std::mutex mutex_2;
};

// unlocks the guard of its caller for its lifetime
class ReverseLock {
 public:
  explicit ReverseLock(std::unique_lock<std::mutex>& l) : l_(l) { l_.unlock(); }
  ~ReverseLock() { l_.lock(); }

 private:
  std::unique_lock<std::mutex>& l_;
};

class ReverseLockUser {
 public:
  void FP_lock_in_scope_ok() {
    std::unique_lock<std::mutex> l(mutex_);
    ReverseLock unlocked(l);
    std::lock_guard<std::mutex> g(mutex_);
  }

  void lock_after_scope_bad() {
    std::unique_lock<std::mutex> l(mutex_);
    {
      ReverseLock unlocked(l);
    }
    std::lock_guard<std::mutex> g(mutex_);
  }

 private:
  std::mutex mutex_;
};

// unlocks the guard of its caller without keeping it
class Releaser {
 public:
  explicit Releaser(std::unique_lock<std::mutex>& l) { l.unlock(); }
};

class ReleaserUser {
 public:
  void lock_after_release_ok() {
    std::unique_lock<std::mutex> l(mutex_);
    Releaser released(l);
    std::lock_guard<std::mutex> g(mutex_);
  }

 private:
  std::mutex mutex_;
};

// keeps the guard it unlocks to relock it later
class Park {
 public:
  void park_unpark_bad() {
    std::unique_lock<std::mutex> l(mutex_);
    park(l);
    unpark();
    std::lock_guard<std::mutex> g(mutex_);
  }

  void park_through_helper_bad() {
    std::unique_lock<std::mutex> l(mutex_);
    park_with_helper(l);
    unpark();
    std::lock_guard<std::mutex> g(mutex_);
  }

 private:
  void park(std::unique_lock<std::mutex>& l) {
    parked_ = &l;
    l.unlock();
  }

  void keep(std::unique_lock<std::mutex>& l) { parked_ = &l; }

  void park_with_helper(std::unique_lock<std::mutex>& l) {
    keep(l);
    l.unlock();
  }

  void unpark() { parked_->lock(); }

  std::mutex mutex_;
  std::unique_lock<std::mutex>* parked_;
};
} // namespace guard_by_reference
