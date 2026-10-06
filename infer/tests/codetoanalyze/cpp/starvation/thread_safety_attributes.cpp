/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mutex>

// clang thread safety attributes on functions defined in another translation
// unit, see https://clang.llvm.org/docs/ThreadSafetyAnalysis.html
#define CAPABILITY(x) __attribute__((capability(x)))
#define SCOPED_CAPABILITY __attribute__((scoped_lockable))
#define ACQUIRE(...) __attribute__((acquire_capability(__VA_ARGS__)))
#define RELEASE(...) __attribute__((release_capability(__VA_ARGS__)))

namespace thread_safety_attributes {

class CAPABILITY("mutex") Mutex {
 public:
  void Lock() ACQUIRE();
  void Unlock() RELEASE();
};

class SCOPED_CAPABILITY MutexLock {
 public:
  explicit MutexLock(Mutex* mu) ACQUIRE(mu);
  ~MutexLock() RELEASE();
};

class SCOPED_CAPABILITY ReleasableMutexLock {
 public:
  explicit ReleasableMutexLock(Mutex* mu) ACQUIRE(mu);
  ~ReleasableMutexLock() RELEASE();
  void Release() RELEASE();
};

void lock_mutex(Mutex* mu) ACQUIRE(mu);
void unlock_mutex(Mutex* mu) RELEASE(mu);

struct MutexPair {
  Mutex first;
  Mutex second;
};

void lock_first(MutexPair* pair) ACQUIRE(pair->first);
void unlock_first(MutexPair* pair) RELEASE(pair->first);

extern std::mutex global_mutex;
void lock_global() ACQUIRE(global_mutex);
void unlock_global() RELEASE(global_mutex);

// the helpers and std::lock_guard lock the same mutex
class Helpers {
 public:
  void thread1_bad() {
    lock_1();
    std::lock_guard<std::mutex> lock2(mutex_2);
    unlock_1();
  }

  void thread2_bad() {
    std::lock_guard<std::mutex> lock2(mutex_2);
    lock_1();
    unlock_1();
  }

  void released_ok() {
    lock_1();
    unlock_1();
    std::lock_guard<std::mutex> lock2(mutex_2);
  }

 private:
  void lock_1() ACQUIRE(mutex_1);
  void unlock_1() RELEASE(mutex_1);

  std::mutex mutex_1;
  std::mutex mutex_2;
};

class CapabilityMethods {
 public:
  void thread1_bad() {
    mutex_1.Lock();
    mutex_2.Lock();
    mutex_2.Unlock();
    mutex_1.Unlock();
  }

  void thread2_bad() {
    mutex_2.Lock();
    mutex_1.Lock();
    mutex_1.Unlock();
    mutex_2.Unlock();
  }

 private:
  Mutex mutex_1;
  Mutex mutex_2;
};

class ScopedCapability {
 public:
  void thread1_bad() {
    MutexLock lock1(&mutex_1);
    MutexLock lock2(&mutex_2);
  }

  void thread2_bad() {
    MutexLock lock2(&mutex_2);
    MutexLock lock1(&mutex_1);
  }

  void sequential_ok() {
    {
      MutexLock lock1(&mutex_1);
    }
    MutexLock lock2(&mutex_2);
  }

  void released_ok() {
    ReleasableMutexLock lock1(&mutex_1);
    lock1.Release();
    MutexLock lock2(&mutex_2);
  }

 private:
  Mutex mutex_1;
  Mutex mutex_2;
};

class Parameters {
 public:
  void thread1_bad() {
    lock_mutex(&mutex_1);
    mutex_2.Lock();
    mutex_2.Unlock();
    unlock_mutex(&mutex_1);
  }

  void thread2_bad() {
    mutex_2.Lock();
    mutex_1.Lock();
    mutex_1.Unlock();
    mutex_2.Unlock();
  }

 private:
  Mutex mutex_1;
  Mutex mutex_2;
};

class FieldOfParameter {
 public:
  void thread1_bad() {
    lock_first(&pair_);
    pair_.second.Lock();
    pair_.second.Unlock();
    unlock_first(&pair_);
  }

  void thread2_bad() {
    pair_.second.Lock();
    pair_.first.Lock();
    pair_.first.Unlock();
    pair_.second.Unlock();
  }

 private:
  MutexPair pair_;
};

// a global capability has no lock path
class Global {
 public:
  void FN_thread1_bad() {
    lock_global();
    std::lock_guard<std::mutex> lock(mutex_);
    unlock_global();
  }

  void FN_thread2_bad() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::lock_guard<std::mutex> global_lock(global_mutex);
  }

 private:
  std::mutex mutex_;
};

} // namespace thread_safety_attributes
