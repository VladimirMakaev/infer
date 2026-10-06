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
    set_locked_bad(x);
  }

 protected:
  // An unlocked derived caller must prevent treating this helper as private.
  void set_locked_bad(int x) {
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
  void set_unlocked(int x) { set_locked_bad(x); }
};

class LockedDerived : public AllCallersLocked {
 public:
  void set_guarded_ok(int x) { set(x); }
};

class IndirectBase {
 public:
  void set(int x) {
    std::lock_guard<std::mutex> lock(mutex_);
    set_locked_bad(x);
  }

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

class IndirectDerived : public IndirectBase {
 public:
  void set_unlocked_bad(int x) { through_private(x); }

 private:
  void through_private(int x) { set_locked_bad(x); }
};

class ContractBase {
 public:
  void set(int x) {
    std::lock_guard<std::mutex> lock(mutex_);
    set_locked_ok(x);
  }

 protected:
  void set_locked_ok(int x) {
    last_ = x_;
    x_ = x;
  }
  std::mutex mutex_;

 private:
  int x_;
  int last_;
};

class ContractDerived : public ContractBase {
 public:
  void set_requires_lock_ok(int x)
      __attribute__((requires_capability(mutex_))) {
    set_locked_ok(x);
  }
};
} // namespace protected_methods
