/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mutex>

namespace protected_methods {

class AllCallersLocked {
 public:
  void set(int x) {
    std::lock_guard<std::mutex> lock(mutex_);
    set_locked_ok(x);
  }

  void reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    set_locked_ok(0);
  }

  int get() {
    std::lock_guard<std::mutex> lock(mutex_);
    return x_;
  }

 protected:
  void set_locked_ok(int x) {
    last_ = x_;
    x_ = x;
  }

 private:
  std::mutex mutex_;
  int x_;
  int last_;
};

class OneCallerUnlocked {
 public:
  void set(int x) {
    std::lock_guard<std::mutex> lock(mutex_);
    set_locked_bad(x);
  }

  void set_unlocked_bad(int x) { set_locked_bad(x); }

 protected:
  void set_locked_bad(int x) {
    last_ = x_;
    x_ = x;
  }

 private:
  std::mutex mutex_;
  int x_;
  int last_;
};

class NoCallerInClass {
 public:
  void set(int x) {
    std::lock_guard<std::mutex> lock(mutex_);
    x_ = x;
  }

 protected:
  int get_bad() { return x_; }

 private:
  std::mutex mutex_;
  int x_;
};

class PublicHelper {
 public:
  void set(int x) {
    std::lock_guard<std::mutex> lock(mutex_);
    set_locked_bad(x);
  }

  void set_locked_bad(int x) {
    last_ = x_;
    x_ = x;
  }

 private:
  std::mutex mutex_;
  int x_;
  int last_;
};

class Base {
 public:
  void set(int x) {
    std::lock_guard<std::mutex> lock(mutex_);
    FN_set_locked_bad(x);
  }

 protected:
  // only the callers in the class are looked at, not the one in Derived
  void FN_set_locked_bad(int x) {
    last_ = x_;
    x_ = x;
  }

 private:
  std::mutex mutex_;
  int x_;
  int last_;
};

class Derived : public Base {
 public:
  void set_unlocked(int x) { FN_set_locked_bad(x); }
};
} // namespace protected_methods
