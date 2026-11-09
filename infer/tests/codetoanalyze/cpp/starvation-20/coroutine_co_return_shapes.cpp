/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <coroutine>
#include <mutex>

// Shapes where a lock guard is still in scope at co_return. Each one gets its own pair
// of mutexes and its own partner taking them in the opposite order, so the per-pair
// deadlock deduplication cannot let one shape mask another. The guards' destructors run
// at co_return, so every coroutine below inverts against its partner and is reported;
// before that was so, the lock looked held at the exit and the pair was dropped.
namespace co_return_shapes {

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

template <typename T>
struct ValueTask {
  using promise_type = ValueTask<T>;
  bool await_ready() noexcept;
  std::coroutine_handle<> await_suspend(std::coroutine_handle<ValueTask<T>>) noexcept;
  T await_resume() noexcept;
  ValueTask<T> get_return_object();
  std::suspend_always initial_suspend() noexcept { return {}; }
  std::suspend_always final_suspend() noexcept { return {}; }
  void return_value(T);
  void unhandled_exception() {}
};

extern bool cond();

// ---- S1: guards in an OUTER scope, co_return inside an inner block -------------------------
class S1 {
 public:
  void partner() { std::lock_guard<std::mutex> g2(b_); std::lock_guard<std::mutex> g1(a_); }
  Task<void> coro_bad() {
    std::lock_guard<std::mutex> g1(a_);
    std::lock_guard<std::mutex> g2(b_);
    {
      int local = 1;
      (void)local;
      co_return;
    }
  }
 private: std::mutex a_, b_;
};

// ---- S2: co_return inside a loop ------------------------------------------------------------
class S2 {
 public:
  void partner() { std::lock_guard<std::mutex> g2(b_); std::lock_guard<std::mutex> g1(a_); }
  Task<void> coro_bad() {
    std::lock_guard<std::mutex> g1(a_);
    std::lock_guard<std::mutex> g2(b_);
    for (int i = 0; i < 10; i++) {
      if (cond()) {
        co_return;
      }
    }
  }
 private: std::mutex a_, b_;
};

// ---- S3: co_return inside try/catch ----------------------------------------------------------
class S3 {
 public:
  void partner() { std::lock_guard<std::mutex> g2(b_); std::lock_guard<std::mutex> g1(a_); }
  Task<void> coro_bad() {
    std::lock_guard<std::mutex> g1(a_);
    std::lock_guard<std::mutex> g2(b_);
    try {
      co_return;
    } catch (...) {
    }
  }
 private: std::mutex a_, b_;
};

// ---- S4: co_return WITH an operand ------------------------------------------------------------
class S4 {
 public:
  void partner() { std::lock_guard<std::mutex> g2(b_); std::lock_guard<std::mutex> g1(a_); }
  ValueTask<int> coro_bad() {
    std::lock_guard<std::mutex> g1(a_);
    std::lock_guard<std::mutex> g2(b_);
    co_return 7;
  }
 private: std::mutex a_, b_;
};

// ---- S5: three guards live at co_return -------------------------------------------------------
class S5 {
 public:
  void partner() { std::lock_guard<std::mutex> g2(b_); std::lock_guard<std::mutex> g1(a_); }
  Task<void> coro_bad() {
    std::lock_guard<std::mutex> g1(a_);
    std::lock_guard<std::mutex> g2(b_);
    std::lock_guard<std::mutex> g3(c_);
    co_return;
  }
 private: std::mutex a_, b_, c_;
};

// ---- S6: early co_return on ONE branch, fall off the end on the other ---------------------------
class S6 {
 public:
  void partner() { std::lock_guard<std::mutex> g2(b_); std::lock_guard<std::mutex> g1(a_); }
  Task<void> coro_bad() {
    std::lock_guard<std::mutex> g1(a_);
    std::lock_guard<std::mutex> g2(b_);
    if (cond()) {
      co_return;
    }
    int x = 0;
    (void)x;
  }
 private: std::mutex a_, b_;
};

// ---- S7: a co_await before the co_return --------------------------------------------------------
class S7 {
 public:
  void partner() { std::lock_guard<std::mutex> g2(b_); std::lock_guard<std::mutex> g1(a_); }
  Task<void> other();
  Task<void> coro_bad() {
    std::lock_guard<std::mutex> g1(a_);
    std::lock_guard<std::mutex> g2(b_);
    co_await other();
    co_return;
  }
 private: std::mutex a_, b_;
};

// ---- S8 CONTROL: nothing live at co_return; must be UNCHANGED by the fix -------------------------
class S8 {
 public:
  void partner() { std::lock_guard<std::mutex> g2(b_); std::lock_guard<std::mutex> g1(a_); }
  Task<void> coro_bad() {
    {
      std::lock_guard<std::mutex> g1(a_);
      std::lock_guard<std::mutex> g2(b_);
    }
    co_return;
  }
 private: std::mutex a_, b_;
};

// ---- S9 CONTROL: an ordinary function; must be UNCHANGED by the fix -------------------------------
class S9 {
 public:
  void partner() { std::lock_guard<std::mutex> g2(b_); std::lock_guard<std::mutex> g1(a_); }
  void plain_bad() {
    std::lock_guard<std::mutex> g1(a_);
    std::lock_guard<std::mutex> g2(b_);
  }
 private: std::mutex a_, b_;
};

// ---- S10 CONTROL: a coroutine taking only ONE lock; must stay SILENT on both arms ------------------
class S10 {
 public:
  void partner() { std::lock_guard<std::mutex> g2(b_); std::lock_guard<std::mutex> g1(a_); }
  Task<void> coro_ok() {
    std::lock_guard<std::mutex> g1(a_);
    co_return;
  }
 private: std::mutex a_, b_;
};

} // namespace co_return_shapes
