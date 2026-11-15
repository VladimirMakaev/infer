/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <functional>
#include <mutex>

namespace function_member {

// Storing a callback and swapping it from one thread while another invokes it.
// Assigning to a std::function calls its non-const operator=, which without a
// model is recorded as a READ of the member rather than a write, so the
// unlocked invocation has no write to conflict with and the race is invisible.
struct Callback {
  void set(const std::function<void()>& cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    fn_ = cb;
  }

  // operator() is const, so invoking the callback is a read of the member.
  void invoke_bad() { fn_(); }

  // operator bool is const too: testing it reads the member just as calling it
  // does, and races with the same write.
  bool empty_bad() { return !fn_; }

  void invoke_ok() {
    std::lock_guard<std::mutex> lock(mutex_);
    fn_();
  }

 private:
  std::mutex mutex_;
  std::function<void()> fn_;
};

// swap is non-const, so it is the other way to write the member.
struct Swapper {
  void exchange(std::function<void()>& other) {
    std::lock_guard<std::mutex> lock(mutex_);
    fn_.swap(other);
  }

  void invoke_bad() { fn_(); }

  void invoke_ok() {
    std::lock_guard<std::mutex> lock(mutex_);
    fn_();
  }

 private:
  std::mutex mutex_;
  std::function<void()> fn_;
};

// Every access under the lock: nothing to report. This is the negative half of
// the pair, and it fails if the model turns the member into a race whatever the
// caller does.
struct FullyLocked {
  void set(const std::function<void()>& cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    fn_ = cb;
  }

  void invoke_ok() {
    std::lock_guard<std::mutex> lock(mutex_);
    fn_();
  }

 private:
  std::mutex mutex_;
  std::function<void()> fn_;
};

// TRUE-POSITIVE CONTROL. The same shape with an int member, which RacerD
// already reported before std::function was modelled. If this ever goes quiet
// the fixture is measuring something other than the model under test.
struct IntMember {
  void set(int x) {
    std::lock_guard<std::mutex> lock(mutex_);
    x_ = x;
  }

  int get_bad() { return x_; }

  int get_ok() {
    std::lock_guard<std::mutex> lock(mutex_);
    return x_;
  }

 private:
  std::mutex mutex_;
  int x_{0};
};

} // namespace function_member
