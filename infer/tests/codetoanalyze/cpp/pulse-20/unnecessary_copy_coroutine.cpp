/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <coroutine>
#include <string>

namespace unnecessary_copy_coroutine {

struct Task {
  struct promise_type {
    Task get_return_object() { return {}; }
    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void return_void() noexcept {}
    void unhandled_exception() noexcept {}
  };
};

void consume(const std::string& value);

// The caller can change `source` while the coroutine is suspended, so the copy
// is what keeps the value the coroutine read it for: a reference would see the
// change.
Task copy_of_borrowed_across_suspend_ok(const std::string& source) {
  const std::string snapshot = source;
  co_await std::suspend_always{};
  consume(snapshot);
}

// Nothing can run between the copy and the read, so a reference is safe.
void copy_of_borrowed_synchronous_bad(const std::string& source) {
  const std::string snapshot = source;
  consume(snapshot);
}

// The source lives in the coroutine frame, so suspending cannot change it.
Task copy_of_owned_local_across_suspend_bad() {
  std::string owned = "x";
  const std::string snapshot = owned;
  co_await std::suspend_always{};
  consume(snapshot);
}

// The copy is read before the coroutine suspends, so a reference would have
// been read before the caller could change anything.
Task copy_of_borrowed_read_before_suspend_bad(const std::string& source) {
  const std::string snapshot = source;
  consume(snapshot);
  co_await std::suspend_always{};
}

// A by-value parameter is owned by the frame, like the local above.
Task copy_of_by_value_param_across_suspend_bad(std::string owned) {
  const std::string snapshot = owned;
  co_await std::suspend_always{};
  consume(snapshot);
}

} // namespace unnecessary_copy_coroutine
