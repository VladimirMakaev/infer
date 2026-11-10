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

// Each shape gets its own pair of mutexes and its own partner taking them in the opposite
// order. A deadlock is deduplicated per file and canonical lock pair, so shapes sharing one
// pair collapse into a single row and a regression in any of them hides behind the others.
namespace coroutine_locks {

class C {
 public:
  // ---- guards still in scope at co_return ----------------------------------------------------
  // Their destructors run there, so the locks are released and the ordering is checked like
  // anywhere else; before the destructors were skipped, the locks looked never-released and the
  // pair was dropped as unbalanced.
  void partner_live_bad() {
    std::lock_guard<std::mutex> g2(live_2_);
    std::lock_guard<std::mutex> g1(live_1_);
  }

  Task<void> coroutine_guards_live_at_co_return_bad() {
    std::lock_guard<std::mutex> g1(live_1_);
    std::lock_guard<std::mutex> g2(live_2_);
    co_return;
  }

  // ---- the same ordering, with the guards released before co_return ---------------------------
  void partner_released_bad() {
    std::lock_guard<std::mutex> g2(released_2_);
    std::lock_guard<std::mutex> g1(released_1_);
  }

  Task<void> coroutine_guards_released_bad() {
    {
      std::lock_guard<std::mutex> g1(released_1_);
      std::lock_guard<std::mutex> g2(released_2_);
    }
    co_return;
  }

  // ---- the same body in an ordinary function --------------------------------------------------
  void partner_plain_bad() {
    std::lock_guard<std::mutex> g2(plain_2_);
    std::lock_guard<std::mutex> g1(plain_1_);
  }

  void not_a_coroutine_bad() {
    std::lock_guard<std::mutex> g1(plain_1_);
    std::lock_guard<std::mutex> g2(plain_2_);
  }

  // ---- only ever takes one lock, so it cannot invert with anything -----------------------------
  // Its own mutex, so that if it ever took a second one the report could not be merged into a
  // shape above and disappear.
  void partner_single() {
    std::lock_guard<std::mutex> g2(single_2_);
    std::lock_guard<std::mutex> g1(single_1_);
  }

  Task<void> coroutine_single_lock_ok() {
    std::lock_guard<std::mutex> g(single_1_);
    co_return;
  }

 private:
  std::mutex live_1_;
  std::mutex live_2_;
  std::mutex released_1_;
  std::mutex released_2_;
  std::mutex plain_1_;
  std::mutex plain_2_;
  std::mutex single_1_;
  std::mutex single_2_;
};

} // namespace coroutine_locks
