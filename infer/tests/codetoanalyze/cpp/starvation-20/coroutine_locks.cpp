/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <coroutine>
#include <mutex>

template <typename T>
struct Task {
  using promise_type = Task<T>;
  bool await_ready() noexcept;
  std::coroutine_handle<> await_suspend(std::coroutine_handle<Task<T>>) noexcept;
  void await_resume() noexcept;
  Task<T> get_return_object();
  std::suspend_always initial_suspend() noexcept { return {}; }
  std::suspend_always final_suspend() noexcept { return {}; }
  void return_void();
  void unhandled_exception() {}
};

namespace coroutine_locks {

class C {
 public:
  // takes the two locks in the opposite order to everything below
  void partner_bad() {
    std::lock_guard<std::mutex> g2(mutex_2_);
    std::lock_guard<std::mutex> g1(mutex_1_);
  }

  // The guards are still in scope at co_return. Their destructors run there, so the locks are
  // released and the ordering is checked like anywhere else; before the destructors were
  // skipped, the locks looked never-released and the pair was dropped as unbalanced.
  Task<void> coroutine_guards_live_at_co_return_bad() {
    std::lock_guard<std::mutex> g1(mutex_1_);
    std::lock_guard<std::mutex> g2(mutex_2_);
    co_return;
  }

  // the same ordering, with the guards released before co_return
  Task<void> coroutine_guards_released_bad() {
    {
      std::lock_guard<std::mutex> g1(mutex_1_);
      std::lock_guard<std::mutex> g2(mutex_2_);
    }
    co_return;
  }

  // the same body in an ordinary function
  void not_a_coroutine_bad() {
    std::lock_guard<std::mutex> g1(mutex_1_);
    std::lock_guard<std::mutex> g2(mutex_2_);
  }

  // only ever takes one lock, so it cannot invert with anything
  Task<void> coroutine_single_lock_ok() {
    std::lock_guard<std::mutex> g(mutex_1_);
    co_return;
  }

 private:
  std::mutex mutex_1_;
  std::mutex mutex_2_;
};

} // namespace coroutine_locks
