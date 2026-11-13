/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <coroutine>
#include <memory>
#include <vector>

namespace const_refable_coroutine {

struct Task {
  struct promise_type {
    Task get_return_object() { return {}; }
    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    std::suspend_always yield_value(int) noexcept { return {}; }
    void return_void() noexcept {}
    void unhandled_exception() noexcept {}
  };
};

struct Api {
  int value;
};

void read(int);
void read_size(unsigned long);

// A by-value parameter of a coroutine is copied into the coroutine frame, and
// that copy is what keeps the argument alive while the coroutine is suspended.
// Taking it by const reference would bind the caller's object instead, which
// may be gone by the time the coroutine resumes, so the advice cannot be
// followed here. `initial_suspend` returning `suspend_always` makes the point
// sharply: nothing in the body runs at the call, so not even the first read is
// guaranteed to happen while the caller's argument is still alive.
Task shared_ptr_across_suspend_ok(std::shared_ptr<Api> self) {
  co_await std::suspend_always{};
  read(self->value);
}

// The same, for a coroutine that never awaits. Recording suspension points is
// not enough to recognise one: this body has none, and is still a coroutine
// whose parameter is copied into the frame.
Task shared_ptr_co_return_only_ok(std::shared_ptr<Api> self) {
  read(self->value);
  co_return;
}

// A coroutine that yields rather than awaits.
Task shared_ptr_co_yield_ok(std::shared_ptr<Api> self) {
  co_yield self->value;
  read(self->value);
}

// The parameter kind does not matter: any by-value parameter of a coroutine is
// held by the frame.
Task vector_across_suspend_ok(std::vector<int> values) {
  co_await std::suspend_always{};
  read_size(values.size());
}

// A method can be a coroutine too.
struct Service {
  Task fetch_ok(std::shared_ptr<Api> self) {
    co_await std::suspend_always{};
    read(self->value);
  }
};

// Controls. An ordinary function is not interrupted between the call and the
// read, so the caller's object is still there and the advice is sound.
void shared_ptr_synchronous_bad(std::shared_ptr<Api> self) {
  read(self->value);
}

void vector_synchronous_bad(std::vector<int> values) {
  read_size(values.size());
}

// The discriminating control: this returns the coroutine's type but is not a
// coroutine, so its parameter is an ordinary one and is still reported. A check
// that asked about the return type rather than the body would lose this.
Task returns_task_but_not_a_coroutine_bad(std::shared_ptr<Api> self) {
  read(self->value);
  return {};
}

} // namespace const_refable_coroutine
