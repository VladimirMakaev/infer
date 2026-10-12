/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#define REQUIRE_CAPABILITY(...) \
  __attribute__((requires_capability(__VA_ARGS__)))

namespace required_capability {

class __attribute__((capability("mutex"))) Mutex;
void acquire(Mutex* mutex) __attribute__((acquire_capability(mutex)));
void release(Mutex* mutex) __attribute__((release_capability(mutex)));

// The direct lock use is on another field: the value's writer only requires
// the capability. Its unguarded reader must still be checked.
class RequiredWriter {
 public:
  explicit RequiredWriter(Mutex* mutex) : mutex_(mutex) {}

  void write_locked_ok() REQUIRE_CAPABILITY(mutex_) { value_++; }
  void mark_lock_use() {
    acquire(mutex_);
    other_++;
    release(mutex_);
  }
  int read_unlocked_bad() { return value_; }

 private:
  Mutex* mutex_;
  int value_ = 0;
  int other_ = 0;
};

class ExplicitWriter {
 public:
  explicit ExplicitWriter(Mutex* mutex) : mutex_(mutex) {}

  void write_locked_ok() {
    acquire(mutex_);
    value_++;
    release(mutex_);
  }
  int read_unlocked_bad() { return value_; }

 private:
  Mutex* mutex_;
  int value_ = 0;
};

class GuardedReader {
 public:
  explicit GuardedReader(Mutex* mutex) : mutex_(mutex) {}

  void write_locked_ok() REQUIRE_CAPABILITY(mutex_) { value_++; }
  void mark_lock_use() {
    acquire(mutex_);
    other_++;
    release(mutex_);
  }
  int read_guarded_ok() {
    acquire(mutex_);
    int value = value_;
    release(mutex_);
    return value;
  }

 private:
  Mutex* mutex_;
  int value_ = 0;
  int other_ = 0;
};

class RequiredReader {
 public:
  explicit RequiredReader(Mutex* mutex) : mutex_(mutex) {}

  void write_locked_ok() REQUIRE_CAPABILITY(mutex_) { value_++; }
  void mark_lock_use() {
    acquire(mutex_);
    other_++;
    release(mutex_);
  }
  int read_required_ok() REQUIRE_CAPABILITY(mutex_) { return value_; }

 private:
  Mutex* mutex_;
  int value_ = 0;
  int other_ = 0;
};

// A defined writer requiring a lock establishes the same contract as an
// explicit acquisition, even when no class method directly acquires it.
class NoDirectLock {
 public:
  explicit NoDirectLock(Mutex* mutex) : mutex_(mutex) {}

  void write_locked_ok() REQUIRE_CAPABILITY(mutex_) { value_++; }
  int read_no_direct_lock_bad() { return value_; }

 private:
  Mutex* mutex_;
  int value_ = 0;
};

class PrivateReader {
 public:
  explicit PrivateReader(Mutex* mutex) : mutex_(mutex) {}

  void write_locked_ok() REQUIRE_CAPABILITY(mutex_) { value_++; }
  void mark_lock_use() {
    acquire(mutex_);
    other_++;
    release(mutex_);
  }
  int read_guarded_ok() {
    acquire(mutex_);
    int value = read_private();
    release(mutex_);
    return value;
  }

 private:
  int read_private() { return value_; }
  Mutex* mutex_;
  int value_ = 0;
  int other_ = 0;
};

// The callee's summary must not assume its required lock is held by callers.
class RequiredReadCall {
 public:
  explicit RequiredReadCall(Mutex* mutex) : mutex_(mutex) {}

  void write_locked_ok() {
    acquire(mutex_);
    value_++;
    release(mutex_);
  }
  int read_locked_ok() REQUIRE_CAPABILITY(mutex_) { return value_; }
  int read_call_unlocked_bad() { return read_locked_ok(); }
  int read_call_guarded_ok() {
    acquire(mutex_);
    int value = read_locked_ok();
    release(mutex_);
    return value;
  }

 private:
  Mutex* mutex_;
  int value_ = 0;
};

// Protected helpers reached only while the caller's required lock is held
// remain protected, just as when their callers acquire it explicitly.
class ProtectedRequiredCaller {
 public:
  explicit ProtectedRequiredCaller(Mutex* mutex) : mutex_(mutex) {}

  void write_locked_ok() REQUIRE_CAPABILITY(mutex_) { value_++; }
  void mark_lock_use() {
    acquire(mutex_);
    other_++;
    release(mutex_);
  }
  int read_required_ok() REQUIRE_CAPABILITY(mutex_) { return read_helper_ok(); }

 protected:
  int read_helper_ok() { return value_; }

 private:
  Mutex* mutex_;
  int value_ = 0;
  int other_ = 0;
};

class ProtectedGuardedCaller {
 public:
  explicit ProtectedGuardedCaller(Mutex* mutex) : mutex_(mutex) {}

  void write_locked_ok() REQUIRE_CAPABILITY(mutex_) { value_++; }
  void mark_lock_use() {
    acquire(mutex_);
    other_++;
    release(mutex_);
  }
  int read_guarded_ok() {
    acquire(mutex_);
    int value = read_helper_ok();
    release(mutex_);
    return value;
  }

 protected:
  int read_helper_ok() { return value_; }

 private:
  Mutex* mutex_;
  int value_ = 0;
  int other_ = 0;
};

// One required caller cannot hide another caller that holds no lock.
class MixedProtectedCallers {
 public:
  explicit MixedProtectedCallers(Mutex* mutex) : mutex_(mutex) {}

  void write_locked_ok() {
    acquire(mutex_);
    value_++;
    release(mutex_);
  }
  int read_required_ok() REQUIRE_CAPABILITY(mutex_) {
    return read_helper_bad();
  }
  int read_call_unlocked_bad() { return read_helper_bad(); }

 protected:
  int read_helper_bad() { return value_; }

 private:
  Mutex* mutex_;
  int value_ = 0;
};

} // namespace required_capability
