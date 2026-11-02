/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// a [folly::Synchronized] whose mutex provides no mutual exclusion must not create lock order
// edges, while one whose mutex we model must keep creating them

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

// the no-op mutex contributes no lock, so these two orders cannot deadlock
class NullMutexOrder {
 public:
  void sync_then_mutex_ok() {
    folly::LockedPtr<folly::Synchronized<Data, NullMutex>, void> guard(&data_);
    std::lock_guard<std::mutex> l(mutex_);
  }

  void mutex_then_sync_ok() {
    std::lock_guard<std::mutex> l(mutex_);
    folly::LockedPtr<folly::Synchronized<Data, NullMutex>, void> guard(&data_);
  }

 private:
  folly::Synchronized<Data, NullMutex> data_;
  std::mutex mutex_;
};

// the default mutex is [folly::SharedMutex], which we model, so the inversion still deadlocks
class DefaultMutexOrder {
 public:
  void sync_then_mutex_bad() {
    folly::LockedPtr<folly::Synchronized<Data>, void> guard(&data_);
    std::lock_guard<std::mutex> l(mutex_);
  }

  void mutex_then_sync_bad() {
    std::lock_guard<std::mutex> l(mutex_);
    folly::LockedPtr<folly::Synchronized<Data>, void> guard(&data_);
  }

 private:
  folly::Synchronized<Data> data_;
  std::mutex mutex_;
};

} // namespace synchronized_null_mutex
