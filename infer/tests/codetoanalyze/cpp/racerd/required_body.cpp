/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#define BODY_REQUIRES(...) \
  __attribute__((requires_capability(__VA_ARGS__)))

namespace required_body {
class __attribute__((capability("mutex"))) Mutex;
void acquire(Mutex* mutex) __attribute__((acquire_capability(mutex)));
void release(Mutex* mutex) __attribute__((release_capability(mutex)));

class Writer {
  Mutex* mutex_;
  int value_ = 0;

 public:
  explicit Writer(Mutex* mutex) : mutex_(mutex) {}
  void write_required_ok() BODY_REQUIRES(mutex_) { value_++; }
  int read_unlocked_bad() { return value_; }
};

class RequiredReader {
  Mutex* mutex_;
  int value_ = 0;

 public:
  explicit RequiredReader(Mutex* mutex) : mutex_(mutex) {}
  void write_required_ok() BODY_REQUIRES(mutex_) { value_++; }
  int read_required_ok() BODY_REQUIRES(mutex_) { return value_; }
};

class GuardedReader {
  Mutex* mutex_;
  int value_ = 0;

 public:
  explicit GuardedReader(Mutex* mutex) : mutex_(mutex) {}
  void write_required_ok() BODY_REQUIRES(mutex_) { value_++; }
  int read_guarded_ok() {
    acquire(mutex_);
    int value = value_;
    release(mutex_);
    return value;
  }
};

class PrivateWriter {
  Mutex* mutex_;
  int value_ = 0;
  void write_private() BODY_REQUIRES(mutex_) { value_++; }

 public:
  explicit PrivateWriter(Mutex* mutex) : mutex_(mutex) {}
  void write_required_ok() BODY_REQUIRES(mutex_) { write_private(); }
  int read_unlocked_bad() { return value_; }
};

class ProtectedReader {
  Mutex* mutex_;
  int value_ = 0;

 protected:
  int read_helper_ok() { return value_; }

 public:
  explicit ProtectedReader(Mutex* mutex) : mutex_(mutex) {}
  void write_required_ok() BODY_REQUIRES(mutex_) { value_++; }
  int read_required_ok() BODY_REQUIRES(mutex_) { return read_helper_ok(); }
};

class MixedProtectedReader {
  Mutex* mutex_;
  int value_ = 0;

 protected:
  int read_helper_bad() { return value_; }

 public:
  explicit MixedProtectedReader(Mutex* mutex) : mutex_(mutex) {}
  void write_required_ok() BODY_REQUIRES(mutex_) { value_++; }
  int read_required_ok() BODY_REQUIRES(mutex_) { return read_helper_bad(); }
  int read_unlocked_bad() { return read_helper_bad(); }
};

class DeclaredWriter {
  Mutex* mutex_;
  int value_ = 0;

 public:
  explicit DeclaredWriter(Mutex* mutex) : mutex_(mutex) {}
  void write_required_ok() BODY_REQUIRES(mutex_);
  int read_unlocked_bad() { return value_; }
};
void DeclaredWriter::write_required_ok() { value_++; }

// Releasing one of two entry locks still leaves the write protected.
class TwoRequiredLocks {
  Mutex* first_;
  Mutex* second_;
  int value_ = 0;

 public:
  TwoRequiredLocks(Mutex* first, Mutex* second)
      : first_(first), second_(second) {}
  void write_required_ok() BODY_REQUIRES(first_, second_) {
    release(first_);
    value_++;
  }
  int read_unlocked_bad() { return value_; }
};

// A read-only requirement must not manufacture a protected write.
class ReadOnly {
  Mutex* mutex_;
  int value_ = 0;

 public:
  explicit ReadOnly(Mutex* mutex) : mutex_(mutex) {}
  int read_required_ok() BODY_REQUIRES(mutex_) { return value_; }
  int read_immutable_ok() { return value_; }
};

// Without an available body there is no evidence that the method writes value_.
class OpaqueWriter {
  Mutex* mutex_;
  int value_ = 0;

 public:
  explicit OpaqueWriter(Mutex* mutex) : mutex_(mutex) {}
  void write_required() BODY_REQUIRES(mutex_);
  int FN_body_unavailable_bad() { return value_; }
};

// Existing limitation: C++ reports require a protected write. The write below
// is unprotected after releasing its entry lock; it must not become protected
// merely because its method has a requirement.
class ReleasedWriter {
  Mutex* mutex_;
  int value_ = 0;

 public:
  explicit ReleasedWriter(Mutex* mutex) : mutex_(mutex) {}
  void write_released() BODY_REQUIRES(mutex_) {
    release(mutex_);
    value_++;
  }
  int FN_released_write_bad() { return value_; }
};

// These components are serialized by their owner. A caller-supplied capability
// does not make their methods independent concurrent entry points.
class ExternalComponent {
  int value_ = 0;

 public:
  void write(Mutex* mutex) BODY_REQUIRES(mutex) { value_++; }
  int read_ok() { return value_; }
};

// The formal shadows a field with the same name. The requirement refers to the
// caller's argument, so a field-name match alone would incorrectly activate it.
class ShadowedComponent {
  Mutex* mutex_ = nullptr;
  int value_ = 0;

 public:
  void write(Mutex* mutex_) BODY_REQUIRES(mutex_) { value_++; }
  int read_ok() { return value_; }
};

class ExternalOwner {
  Mutex* mutex_;
  ExternalComponent* component_;
  ShadowedComponent* shadowed_;

 public:
  ExternalOwner(Mutex* mutex, ExternalComponent* component,
                ShadowedComponent* shadowed)
      : mutex_(mutex), component_(component), shadowed_(shadowed) {}
  void write_guarded_ok() {
    acquire(mutex_);
    component_->write(mutex_);
    shadowed_->write(mutex_);
    release(mutex_);
  }
  int read_guarded_ok() {
    acquire(mutex_);
    int value = component_->read_ok() + shadowed_->read_ok();
    release(mutex_);
    return value;
  }
};
} // namespace required_body
