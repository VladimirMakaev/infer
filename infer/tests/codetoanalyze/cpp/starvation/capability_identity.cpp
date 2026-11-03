/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mutex>
#include <pthread.h>

namespace capability_identity {

void work();
std::mutex auxiliary;

template <class M>
class __attribute__((capability("mutex"))) Mutex {
 public:
  void lock() __attribute__((acquire_capability())) { inner.lock(); }
  void unlock() __attribute__((release_capability())) { inner.unlock(); }
  // The auxiliary acquisition must survive normalization of the wrapped mutex.
  void lock_with_auxiliary() {
    inner.lock();
    auxiliary.lock();
    auxiliary.unlock();
  }
  void auxiliary_order_control_bad() {
    inner.lock();
    auxiliary.lock();
    auxiliary.unlock();
    inner.unlock();
  }
  M inner;
};

template <class M>
class __attribute__((scoped_lockable)) Guard {
 public:
  explicit Guard(M& mutex) __attribute__((acquire_capability(mutex)))
      : guard(mutex.inner) {}
  ~Guard() __attribute__((release_capability())) = default;

 private:
  std::unique_lock<decltype(M::inner)> guard;
};

using PlainMutex = Mutex<std::mutex>;
using RecursiveMutex = Mutex<std::recursive_mutex>;

// The two guard kinds must name the same two locks in both directions.
class Mixed {
 public:
  void forward_bad() {
    Guard<PlainMutex> a(first);
    std::lock_guard<PlainMutex> b(second);
    work();
  }
  void reverse_bad() {
    std::lock_guard<PlainMutex> b(second);
    Guard<PlainMutex> a(first);
    work();
  }
  void direct_reverse_bad() {
    second.lock();
    first.lock();
    work();
    first.unlock();
    second.unlock();
  }

 private:
  PlainMutex first, second;
};

class Consistent {
 public:
  void guarded_ok() {
    Guard<PlainMutex> a(first);
    std::lock_guard<PlainMutex> b(second);
    work();
  }
  void direct_ok() {
    first.lock();
    second.lock();
    second.unlock();
    first.unlock();
  }

 private:
  PlainMutex first, second;
};

class Self {
 public:
  void mixed_bad() {
    Guard<PlainMutex> guard(mutex);
    mutex.lock();
    mutex.unlock();
  }
  void direct_control_bad() {
    mutex.lock();
    mutex.lock();
    mutex.unlock();
    mutex.unlock();
  }
  void release_retake_ok() {
    mutex.lock();
    mutex.unlock();
    Guard<PlainMutex> guard(mutex);
    work();
  }

 private:
  PlainMutex mutex;
};

class Recursive {
 public:
  void mixed_ok() {
    Guard<RecursiveMutex> guard(mutex);
    mutex.lock();
    mutex.unlock();
  }

 private:
  RecursiveMutex mutex;
};

// Distinct full owner paths must remain distinct even with the same last field.
struct Owner {
  PlainMutex mutex;
};
class Nested {
 public:
  void forward_bad() {
    Guard<PlainMutex> a(left.mutex);
    right.mutex.lock();
    right.mutex.unlock();
  }
  void reverse_bad() {
    Guard<PlainMutex> b(right.mutex);
    left.mutex.lock();
    left.mutex.unlock();
  }

 private:
  Owner left, right;
};

class CommonOuter {
 public:
  void forward_ok() {
    std::lock_guard<std::mutex> held(outer);
    Guard<PlainMutex> a(first);
    second.lock();
    second.unlock();
  }
  void reverse_ok() {
    std::lock_guard<std::mutex> held(outer);
    Guard<PlainMutex> b(second);
    first.lock();
    first.unlock();
  }

 private:
  std::mutex outer;
  PlainMutex first, second;
};

// Both arms acquire the auxiliary lock under the same wrapped mutex.
// Canonicalizing that mutex must retain the auxiliary edge and its inversion.
class Auxiliary {
 public:
  void forward_bad() {
    Guard<PlainMutex> a(mutex);
    auxiliary.lock();
    auxiliary.unlock();
  }
  void reverse_bad() {
    std::lock_guard<std::mutex> a(auxiliary);
    mutex.lock_with_auxiliary();
    mutex.unlock();
  }

 private:
  PlainMutex mutex;
};

// A method on a capability and a method on its containing object may access the
// same inline mutex. The alias test must retain that relation after renaming.
class CrossRootControl {
 public:
  void forward_bad() { mutex.auxiliary_order_control_bad(); }
  void reverse_bad() {
    std::lock_guard<std::mutex> held(auxiliary);
    mutex.lock();
    mutex.unlock();
  }

 private:
  PlainMutex mutex;
};

class __attribute__((capability("mutex"))) RuntimeRecursiveMutex {
 public:
  RuntimeRecursiveMutex() {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&inner, &attr);
    pthread_mutexattr_destroy(&attr);
  }
  void lock() { pthread_mutex_lock(&inner); }
  void unlock() { pthread_mutex_unlock(&inner); }

 private:
  pthread_mutex_t inner;
};

class RuntimeRecursiveControl {
 public:
  void twice_ok() {
    mutex.lock();
    mutex.lock();
    mutex.unlock();
    mutex.unlock();
  }

 private:
  RuntimeRecursiveMutex mutex;
};

} // namespace capability_identity
