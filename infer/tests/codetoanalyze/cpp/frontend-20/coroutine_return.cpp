/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Minimal coroutine declarations keep this CFG independent of library
// implementations.
namespace std {
template <typename R, typename... Args>
struct coroutine_traits {
  using promise_type = typename R::promise_type;
};
template <typename Promise = void>
struct coroutine_handle {
  static coroutine_handle from_address(void*) noexcept;
};
} // namespace std
namespace coroutine_once {
struct Suspend {
  bool await_ready() const noexcept;
  template <typename Promise>
  void await_suspend(std::coroutine_handle<Promise>) const noexcept;
  void await_resume() const noexcept;
};
struct Task {
  struct promise_type {
    Task get_return_object();
    Suspend initial_suspend() noexcept;
    Suspend final_suspend() noexcept;
    void return_value(int);
    void unhandled_exception();
  };
  bool await_ready() const;
  template <typename Promise>
  void await_suspend(std::coroutine_handle<Promise>) const;
  int await_resume() const;
};
struct VoidTask {
  struct promise_type {
    VoidTask get_return_object();
    Suspend initial_suspend() noexcept;
    Suspend final_suspend() noexcept;
    void return_void();
    void unhandled_exception();
  };
  bool await_ready() const;
  template <typename Promise>
  void await_suspend(std::coroutine_handle<Promise>) const;
  void await_resume() const;
};
[[noreturn]] int always_throw();
Task source();
VoidTask void_source();
int next_value();
void side_effect();
Task direct_await() { co_return co_await source(); }
Task split_await() {
  int value = co_await source();
  co_return value;
}
Task throwing_effect() { co_return always_throw(); }
Task value_effect() { co_return next_value(); }
Task increment_effect(int& count) { co_return count++; }
VoidTask void_await() { co_return co_await void_source(); }
VoidTask void_effect() { co_return side_effect(); }
VoidTask empty_return() { co_return; }
} // namespace coroutine_once
