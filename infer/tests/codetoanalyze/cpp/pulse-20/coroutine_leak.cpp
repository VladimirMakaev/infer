/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <coroutine>

namespace coroutine_leak {

// Allocates in its constructor and frees in its destructor, like the
// type-erased members a coroutine promise commonly holds.
struct Owner {
  int* data;
  Owner() : data(new int(0)) {}
  ~Owner() { delete data; }
  Owner(const Owner&) = delete;
  Owner& operator=(const Owner&) = delete;
};

struct Task {
  struct promise_type {
    Owner owner;
    Task get_return_object() {
      return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
    }
    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void return_value(int) {}
    void unhandled_exception() {}
  };
  std::coroutine_handle<promise_type> handle;
};

struct VoidTask {
  struct promise_type {
    Owner owner;
    VoidTask get_return_object() {
      return VoidTask{std::coroutine_handle<promise_type>::from_promise(*this)};
    }
    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void return_void() {}
    void unhandled_exception() {}
  };
  std::coroutine_handle<promise_type> handle;
};

// A promise that owns memory is handed to the caller together with the
// coroutine state; the ramp function does not leak it.
Task promise_owns_memory_ok() { co_return 1; }

Task promise_owns_memory_after_await_ok() {
  co_await std::suspend_always{};
  co_return 2;
}

Task promise_owns_memory_early_return_ok(bool early) {
  if (early) {
    co_return 1;
  }
  co_return 2;
}

Task promise_owns_memory_in_loop_ok(int count) {
  for (int i = 0; i < count; i++) {
    if (i == 3) {
      co_return i;
    }
  }
  co_return 0;
}

// The body falls off the end instead of returning: this shape always reached
// the return object, keep it pinned.
VoidTask promise_owns_memory_fallthrough_ok() {
  co_await std::suspend_always{};
}

VoidTask promise_owns_memory_void_return_ok() { co_return; }

// A coroutine that never returns anything to its caller still must not be
// blamed for the memory its promise owns.
VoidTask promise_owns_memory_never_resumes_ok() {
  co_await std::suspend_always{};
  co_await std::suspend_always{};
}

// --- true positive controls: memory the coroutine body itself drops ---

// Allocated inside the coroutine state and never freed by anyone: a genuine
// leak, and the only kind of coroutine-state leak Pulse can see.
Task body_leaks_bad() {
  int* leaked = new int(1);
  co_return 2;
}

Task body_leaks_before_await_bad() {
  int* leaked = new int(1);
  co_await std::suspend_always{};
  co_return 2;
}

VoidTask body_leaks_void_bad() {
  int* leaked = new int(1);
  co_return;
}

Task body_overwrites_allocation_bad() {
  int* leaked = new int(1);
  leaked = new int(2);
  delete leaked;
  co_return 3;
}

// The same allocation, freed: must stay silent.
Task body_frees_ok() {
  int* owned = new int(1);
  delete owned;
  co_return 2;
}

// A plain function control, untouched by coroutine translation.
void plain_leak_bad() { int* leaked = new int(1); }

void plain_no_leak_ok() {
  int* owned = new int(1);
  delete owned;
}

// --- callers of a coroutine: the ramp function hands them a return object,
// --- and they must not inherit the report it used to make ---

void caller_drops_the_task_ok() { Task task = promise_owns_memory_ok(); }

void caller_destroys_the_state_ok() {
  Task task = promise_owns_memory_ok();
  task.handle.destroy();
}

Task caller_forwards_the_task_ok() {
  Task task = promise_owns_memory_ok();
  return task;
}

// The caller's own allocation is still its own business.
void caller_leaks_its_own_allocation_bad() {
  Task task = promise_owns_memory_ok();
  int* leaked = new int(1);
}

} // namespace coroutine_leak
