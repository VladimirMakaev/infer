/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <coroutine>

// Destructors of locals in scope at co_return now run there. These pin both halves of
// that: the leaks a freeing destructor used to be blamed for are gone, and a destructor
// must not run a second time on a path that also leaves the body normally, which would
// free twice.
namespace co_return_destructors {

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

struct Deleter {        // frees in its destructor
  int* p;
  ~Deleter() { delete p; }
};
struct NoisyNoFree {    // a side effect, but it does NOT free
  int* p;
  static int sink;
  ~NoisyNoFree() { sink = 1; }
};
int NoisyNoFree::sink = 0;

// P1  the FP the fix removes: the destructor frees, so there is no leak
Task<void> p1_freed_by_dtor_ok() {
  int* p = new int;
  Deleter d{p};
  co_return;
}

// P2  a REAL leak in a coroutine: nothing frees it. Must report on BOTH arms (no TP lost).
Task<void> p2_real_leak_bad() {
  int* p = new int;
  (void)p;
  co_return;
}

// P3  a destructor with a side effect that does NOT free: still a real leak on both arms.
Task<void> p3_dtor_does_not_free_bad() {
  int* p = new int;
  NoisyNoFree d{p};
  co_return;
}

// P4  THE DOUBLE-RUN PROBE. One branch leaves by co_return, the other falls off the end. If the
//     fix injected the destructor at co_return AND the normal scope exit also ran it, this path
//     would free twice. Must be SILENT on the candidate.
Task<void> p4_cond_co_return_no_double_free_ok() {
  int* p = new int;
  Deleter d{p};
  if (cond()) {
    co_return;
  }
  // falls off the end: the ordinary scope exit destroys d here
}

// P5  the same shape with the co_return inside a loop
Task<void> p5_loop_co_return_no_double_free_ok() {
  int* p = new int;
  Deleter d{p};
  for (int i = 0; i < 4; i++) {
    if (cond()) {
      co_return;
    }
  }
}

// P6  nested scopes: the inner guard is destroyed at the inner brace, the outer at co_return.
//     Neither must be destroyed twice.
Task<void> p6_nested_ok() {
  int* outer = new int;
  Deleter d_outer{outer};
  {
    int* inner = new int;
    Deleter d_inner{inner};
  }
  co_return;
}

// P7  CONTROL, not a coroutine: must be identical on both arms.
void p7_noncoro_freed_ok() {
  int* p = new int;
  Deleter d{p};
}

// P8  CONTROL, a real leak outside a coroutine: must report on both arms.
void p8_noncoro_real_leak_bad() {
  int* p = new int;
  (void)p;
}

// P9  CONTROL use-after-delete at an ordinary inner scope exit: must report on both arms, and
//     proves the probe can see a destructor-caused error at all.
int p9_use_after_scope_dtor_bad() {
  int* p = new int;
  {
    Deleter d{p};
  }
  return *p;
}

} // namespace co_return_destructors
