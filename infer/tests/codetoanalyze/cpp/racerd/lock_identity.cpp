/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#define IDENTITY_REQUIRES(...) \
  __attribute__((requires_capability(__VA_ARGS__)))

namespace lock_identity {
class __attribute__((capability("mutex"))) Mutex {};
void acquire(Mutex* mutex) __attribute__((acquire_capability(mutex)));
void release(Mutex* mutex) __attribute__((release_capability(mutex)));

class SameMutex {
  Mutex* mutex_;
  int value_ = 0;

 public:
  explicit SameMutex(Mutex* mutex) : mutex_(mutex) {}
  void write_guarded_ok() {
    acquire(mutex_);
    value_++;
    release(mutex_);
  }
  int read_guarded_ok() {
    acquire(mutex_);
    int value = value_;
    release(mutex_);
    return value;
  }
};

class DifferentMutexes {
  Mutex first_;
  Mutex second_;
  int value_ = 0;

 public:
  void write_guarded() {
    acquire(&first_);
    value_++;
    release(&first_);
  }
  // Existing FN: these two mutex members are distinct, so the accesses can race.
  // RacerD currently tracks lock counts, not the identities of these locks.
  int FN_read_different_mutex_bad() {
    acquire(&second_);
    int value = value_;
    release(&second_);
    return value;
  }
};

class UnlockedReader {
  Mutex* mutex_;
  int value_ = 0;

 public:
  explicit UnlockedReader(Mutex* mutex) : mutex_(mutex) {}
  void write_guarded_ok() {
    acquire(mutex_);
    value_++;
    release(mutex_);
  }
  int read_unlocked_bad() { return value_; }
};

class SharedCommonMutex {
  Mutex* first_;
  Mutex* common_;
  Mutex* last_;
  int value_ = 0;

 public:
  SharedCommonMutex(Mutex* first, Mutex* common, Mutex* last)
      : first_(first), common_(common), last_(last) {}
  void write_guarded_ok() {
    acquire(first_);
    acquire(common_);
    value_++;
    release(common_);
    release(first_);
  }
  int read_guarded_ok() {
    acquire(common_);
    acquire(last_);
    int value = value_;
    release(last_);
    release(common_);
    return value;
  }
};

class SameRequirement {
  Mutex* mutex_;
  int value_ = 0;

 public:
  explicit SameRequirement(Mutex* mutex) : mutex_(mutex) {}
  void write_required_ok() IDENTITY_REQUIRES(mutex_) { value_++; }
  int read_required_ok() IDENTITY_REQUIRES(mutex_) { return value_; }
};

class DifferentRequirements {
  Mutex first_;
  Mutex second_;
  int value_ = 0;

 public:
  void write_required() IDENTITY_REQUIRES(first_) { value_++; }
  // Existing FN: requiring different locks does not synchronize the accesses.
  int FN_read_different_requirement_bad() IDENTITY_REQUIRES(second_) {
    return value_;
  }
};
} // namespace lock_identity
