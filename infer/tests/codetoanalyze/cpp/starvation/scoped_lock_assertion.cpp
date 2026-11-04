/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mutex>

#define TSA(x) __attribute__((x))

namespace android::base {
class TSA(scoped_lockable) ScopedLockAssertion {
 public:
  template <class Mutex>
  explicit ScopedLockAssertion(Mutex& mutex) TSA(acquire_capability(mutex)) {}
  ~ScopedLockAssertion() TSA(release_capability()) {}
};
} // namespace android::base

namespace scoped_lock_assertion {
class TSA(scoped_lockable) EmptyAssertion {
 public:
  explicit EmptyAssertion(std::mutex& mutex) TSA(acquire_capability(mutex)) {}
  ~EmptyAssertion() TSA(release_capability()) {}
};

// A real guard can have no lock state in its summary: its member does the locking.
class TSA(scoped_lockable) RealGuard {
 public:
  explicit RealGuard(std::mutex& mutex) TSA(acquire_capability(mutex)) : held_(mutex) {}
  ~RealGuard() TSA(release_capability()) = default;

 private:
  std::unique_lock<std::mutex> held_;
};

// Without a captured body, the declared acquisition must remain authoritative.
class TSA(scoped_lockable) ExternalGuard {
 public:
  explicit ExternalGuard(std::mutex& mutex) TSA(acquire_capability(mutex));
  ~ExternalGuard() TSA(release_capability());
};

void android_assertion_after_unique_ok(std::mutex& mutex) {
  std::unique_lock<std::mutex> held(mutex);
  android::base::ScopedLockAssertion assertion(mutex);
}

// Capturing this reproduces assertion checking inside a callback body.
class LambdaClient {
 public:
  void assertion_after_unique_ok() {
    auto use = [this] {
      std::unique_lock<std::mutex> held(mutex_);
      android::base::ScopedLockAssertion assertion(mutex_);
    };
    use();
  }

  void real_guard_after_unique_bad() {
    auto use = [this] {
      std::unique_lock<std::mutex> held(mutex_);
      RealGuard second(mutex_);
    };
    use();
  }

 private:
  std::mutex mutex_;
};

void generic_assertion_after_unique_ok(std::mutex& mutex) {
  std::unique_lock<std::mutex> held(mutex);
  EmptyAssertion assertion(mutex);
}

void assertion_does_not_acquire_ok(std::mutex& mutex) {
  EmptyAssertion assertion(mutex);
  mutex.lock();
  mutex.unlock();
}

void assertion_scope_then_real_guard_ok(std::mutex& mutex) {
  {
    std::unique_lock<std::mutex> held(mutex);
    { EmptyAssertion assertion(mutex); }
  }
  RealGuard held(mutex);
}

void assertion_does_not_release_owner_bad(std::mutex& mutex) {
  std::unique_lock<std::mutex> held(mutex);
  { EmptyAssertion assertion(mutex); }
  mutex.lock();
  mutex.unlock();
}

void real_guard_after_unique_bad(std::mutex& mutex) {
  std::unique_lock<std::mutex> first(mutex);
  RealGuard second(mutex);
}

void real_guard_twice_bad(std::mutex& mutex) {
  RealGuard first(mutex);
  RealGuard second(mutex);
}

void external_guard_after_unique_bad(std::mutex& mutex) {
  std::unique_lock<std::mutex> first(mutex);
  ExternalGuard second(mutex);
}
// Reading a buffer under unique_lock may assert ownership for annotated helpers.
class BufferReader {
 public:
  void latest_buffer_assertion_ok() {
    std::unique_lock<std::mutex> held(mutex_);
    android::base::ScopedLockAssertion assertion(mutex_);
  }

 private:
  std::mutex mutex_;
};

// An assertion in a frame callback must not acquire its member mutex again.
class FrameQueue {
 public:
  void frame_available_assertion_ok() {
    std::unique_lock<std::mutex> held(mutex_);
    android::base::ScopedLockAssertion assertion(mutex_);
  }

 private:
  std::mutex mutex_;
};
} // namespace scoped_lock_assertion
