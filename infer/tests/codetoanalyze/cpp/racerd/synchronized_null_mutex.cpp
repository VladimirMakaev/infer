/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// [folly::LockedPtr] is a built-in guard, but what it locks is the mutex of the
// [folly::Synchronized] it points into. A mutex we have no lock model for may provide no mutual
// exclusion at all, so the guard must not make the data it returns look protected.

#include <mutex>

namespace folly {

template <class SynchronizedType, class LockPolicy>
class LockedPtr {
 public:
  explicit LockedPtr(SynchronizedType* parent) : parent_(parent) {
    parent_->mutex_.lock();
  }
  ~LockedPtr() { parent_->mutex_.unlock(); }

 private:
  SynchronizedType* parent_;
};

class SharedMutex {
 public:
  void lock() {}
  void unlock() {}
};

template <class T, class Mutex = SharedMutex>
struct Synchronized {
  using WLockedPtr = LockedPtr<Synchronized, void>;
  WLockedPtr wlock() { return WLockedPtr(this); }
  T datum_;
  Mutex mutex_;
};

} // namespace folly

namespace synchronized_null_mutex {

// no mutual exclusion at all
struct NullMutex {
  void lock() {}
  void unlock() {}
};

struct Data {
  int value;
};

// the mutex is a no-op, so nothing here is protected and the unsynchronised read of the same
// field from an unlocked method is not a race report, because there is no lock to be
// inconsistent with
class NullMutexHolder {
 public:
  void set_ok(int v) {
    auto locked = data_.wlock();
    data_.datum_.value = v;
  }

  int get_ok() { return data_.datum_.value; }

 private:
  folly::Synchronized<Data, NullMutex> data_;
};

// the default mutex is [folly::SharedMutex], which we do model: unchanged, still a race
class DefaultMutexHolder {
 public:
  void set(int v) {
    auto locked = data_.wlock();
    data_.datum_.value = v;
  }

  int get_bad() { return data_.datum_.value; }

 private:
  folly::Synchronized<Data> data_;
};

// an explicit [std::mutex], which we also model: unchanged, still a race
class StdMutexHolder {
 public:
  void set(int v) {
    auto locked = data_.wlock();
    data_.datum_.value = v;
  }

  int get_bad() { return data_.datum_.value; }

 private:
  folly::Synchronized<Data, std::mutex> data_;
};

// a real mutex with a lock model keeps protecting its data
class ProtectedHolder {
 public:
  void set_ok(int v) {
    std::lock_guard<std::mutex> l(mutex_);
    value_ = v;
  }

  int get_ok() {
    std::lock_guard<std::mutex> l(mutex_);
    return value_;
  }

 private:
  std::mutex mutex_;
  int value_;
};

} // namespace synchronized_null_mutex
