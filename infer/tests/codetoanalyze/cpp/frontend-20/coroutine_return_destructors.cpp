/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <coroutine>

// A local with a destructor, live at `co_return`. The destructor has to run there, just as it
// would on a plain `return`: see the call to ~Guard on the co_return path below.
struct Guard {
  int* p;
  ~Guard();
};

struct Task {
  using promise_type = Task;
  bool await_ready() noexcept;
  std::coroutine_handle<> await_suspend(std::coroutine_handle<Task>) noexcept;
  void await_resume() noexcept;
  Task get_return_object();
  std::suspend_always initial_suspend() noexcept { return {}; }
  std::suspend_always final_suspend() noexcept { return {}; }
  void return_void();
  void unhandled_exception() {}
};

Task destroys_local_at_co_return(int* p) {
  Guard g{p};
  co_return;
}
