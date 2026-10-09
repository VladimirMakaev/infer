/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <coroutine>
#include <memory>
#include <utility>

namespace coroutine_once {
struct Task {
  struct promise_type;
  using Handle = std::coroutine_handle<promise_type>;
  Handle handle;
  explicit Task(Handle value) : handle(value) {}
  Task(Task&& other) : handle(std::exchange(other.handle, {})) {}
  ~Task() {
    if (handle)
      handle.destroy();
  }
  bool await_ready() const { return true; }
  void await_suspend(std::coroutine_handle<>) const {}
  int await_resume() const;
  struct promise_type {
    int result;
    Task get_return_object() { return Task{Handle::from_promise(*this)}; }
    std::suspend_never initial_suspend() { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void return_value(int value) { result = value; }
    void unhandled_exception() {}
  };
};
int Task::await_resume() const { return handle.promise().result; }

Task inner(std::unique_ptr<int> value) { co_return *value; }
int read_once(std::unique_ptr<int> value) { return *value; }

int value_or_throw(bool fail, std::unique_ptr<int> value) {
  if (fail) {
    throw 7;
  }
  return *value;
}

Task direct_await_guarded_ok(std::unique_ptr<int> value) {
  if (!value)
    co_return 0;
  co_return co_await inner(std::move(value));
}
Task split_await_guarded_ok(std::unique_ptr<int> value) {
  if (!value)
    co_return 0;
  int result = co_await inner(std::move(value));
  co_return result;
}
Task direct_value_guarded_ok(std::unique_ptr<int> value) {
  if (!value)
    co_return 0;
  co_return read_once(std::move(value));
}
Task throwing_operand_guarded_ok(bool fail, std::unique_ptr<int> value) {
  if (!value) {
    co_return 0;
  }
  co_return value_or_throw(fail, std::move(value));
}
Task direct_await_null_bad() {
  std::unique_ptr<int> value;
  co_return co_await inner(std::move(value));
}
Task direct_value_null_bad() {
  std::unique_ptr<int> value;
  co_return read_once(std::move(value));
}
Task guarded_null_ok() { return direct_await_guarded_ok(nullptr); }

struct VoidTask {
  struct promise_type {
    VoidTask get_return_object() { return {}; }
    std::suspend_never initial_suspend() { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }
    void return_void() {}
    void unhandled_exception() {}
  };
  bool await_ready() const { return true; }
  void await_suspend(std::coroutine_handle<>) const {}
  void await_resume() const {}
};
VoidTask void_inner(int* value) {
  *value = 17;
  co_return;
}
VoidTask void_await_guarded_ok(int* value) {
  if (!value)
    co_return;
  co_return co_await void_inner(value);
}
VoidTask void_await_null_bad() { co_return co_await void_inner(nullptr); }
VoidTask void_effect_null_bad() {
  int* value = nullptr;
  co_return static_cast<void>(*value = 17);
}
VoidTask void_effect_ok(int& calls) { co_return static_cast<void>(++calls); }
} // namespace coroutine_once
