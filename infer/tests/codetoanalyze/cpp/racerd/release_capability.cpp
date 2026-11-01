/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#define RELEASE_CAPABILITY(...) __attribute__((release_capability(__VA_ARGS__)))
#define RELEASE_SHARED_CAPABILITY(...) \
  __attribute__((release_shared_capability(__VA_ARGS__)))

namespace release_capability {

class __attribute__((capability("mutex"))) Mutex {};
void acquire(Mutex* mutex) __attribute__((acquire_capability(mutex)));
void release(Mutex* mutex) __attribute__((release_capability(mutex)));

// A method that releases a capability is entered holding it, so what it reads
// before giving it up is synchronized.
class Releaser {
 public:
  explicit Releaser(Mutex* mutex) : mutex_(mutex) {}

  void begin() __attribute__((acquire_capability(mutex_))) { acquire(mutex_); }
  int end_ok() RELEASE_CAPABILITY(mutex_) {
    int value = value_;
    release(mutex_);
    return value;
  }
  void write_locked_ok() {
    acquire(mutex_);
    value_++;
    release(mutex_);
  }
  // The capability is only an entry precondition of the method that declares
  // it: a sibling without one is still unsynchronized.
  int read_unlocked_bad() { return value_; }

 private:
  Mutex* mutex_;
  int value_ = 0;
};

// The shared form says the same thing about entry.
class SharedReleaser {
 public:
  explicit SharedReleaser(Mutex* mutex) : mutex_(mutex) {}

  int end_ok() RELEASE_SHARED_CAPABILITY(mutex_) {
    int value = value_;
    release(mutex_);
    return value;
  }
  void write_locked_ok() {
    acquire(mutex_);
    value_++;
    release(mutex_);
  }

 private:
  Mutex* mutex_;
  int value_ = 0;
};

// Releasing at a call site still releases: what the caller touches afterwards
// is not protected.
class CallSiteRelease {
 public:
  explicit CallSiteRelease(Mutex* mutex) : mutex_(mutex) {}

  void write_locked_ok() {
    acquire(mutex_);
    value_++;
    release(mutex_);
  }
  int read_after_release_bad() {
    acquire(mutex_);
    int other = other_;
    release(mutex_);
    return other + value_;
  }

 private:
  Mutex* mutex_;
  int value_ = 0;
  int other_ = 0;
};
} // namespace release_capability
