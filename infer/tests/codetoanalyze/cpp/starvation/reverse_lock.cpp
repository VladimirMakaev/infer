/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mutex>

// a callee that gives its caller's lock up and takes it back
namespace reverse_lock {

using Lock = std::unique_lock<std::mutex>;

// hands the lock back before returning, so keeping the guard does not hide it
class Restores {
 public:
  void release_and_restore_ok() {
    Lock l(mutex_);
    run_released(l);
  }

  void keep_and_take_bad() {
    Lock l(mutex_);
    run_held(l);
  }

  // released on one path only, so the lock is not given back on the other
  void release_on_one_path_bad(bool flag) {
    Lock l(mutex_);
    maybe_release(l, flag);
    std::lock_guard<std::mutex> g(mutex_);
  }

 private:
  void take() { std::lock_guard<std::mutex> g(mutex_); }

  void run_released(Lock& l) {
    kept_ = &l;
    l.unlock();
    take();
    l.lock();
  }

  void run_held(Lock& l) {
    kept_ = &l;
    take();
  }

  void maybe_release(Lock& l, bool flag) {
    kept_ = &l;
    l.unlock();
    if (flag) {
      l.lock();
    }
  }

  std::mutex mutex_;
  Lock* kept_;
};

// gives the lock up and leaves the retaking to another method
class Parks {
 public:
  void park_unpark_bad() {
    Lock l(mutex_);
    park(l);
    unpark();
    std::lock_guard<std::mutex> g(mutex_);
  }

 private:
  void park(Lock& l) {
    parked_ = &l;
    l.unlock();
  }

  void unpark() { parked_->lock(); }

  std::mutex mutex_;
  Lock* parked_;
};

// releases a mutex for its own lifetime and takes it back when destroyed
class ReverseMutex {
 public:
  explicit ReverseMutex(std::mutex& mutex) : mutex_(mutex) { mutex_.unlock(); }
  ~ReverseMutex() { mutex_.lock(); }

 private:
  std::mutex& mutex_;
};

class ReverseMutexUser {
 public:
  void lock_in_scope_ok() {
    Lock l(mutex_);
    ReverseMutex released(mutex_);
    std::lock_guard<std::mutex> g(mutex_);
  }

  void lock_after_scope_bad() {
    Lock l(mutex_);
    { ReverseMutex released(mutex_); }
    std::lock_guard<std::mutex> g(mutex_);
  }

 private:
  std::mutex mutex_;
};

// the same over a guard rather than a mutex: the lock the destructor takes back
// is reached through a field, which a caller cannot name, so this is reported
class ReverseGuard {
 public:
  explicit ReverseGuard(Lock& l) : l_(l) { l_.unlock(); }
  ~ReverseGuard() { l_.lock(); }

 private:
  Lock& l_;
};

class ReverseGuardUser {
 public:
  void FP_lock_in_scope_ok() {
    Lock l(mutex_);
    ReverseGuard released(l);
    std::lock_guard<std::mutex> g(mutex_);
  }

  void lock_after_scope_bad() {
    Lock l(mutex_);
    { ReverseGuard released(l); }
    std::lock_guard<std::mutex> g(mutex_);
  }

 private:
  std::mutex mutex_;
};

} // namespace reverse_lock
