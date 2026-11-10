/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <coroutine>
#include <mutex>

namespace folly {
namespace coro {
// lazy: the body does not start until the task is awaited or scheduled
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
void blockingWait(Task<T>);
// a different function: it does not run the task on the calling thread
template <typename T>
void blockingWaitAsync(Task<T>);

// a task already bound to an executor: awaiting it runs the task THERE, not here, which is
// why it is deliberately not one of the lazy task types
template <typename T>
struct TaskWithExecutor {
  explicit TaskWithExecutor(Task<T> t);
  bool await_ready() noexcept;
  std::coroutine_handle<> await_suspend(std::coroutine_handle<>) noexcept;
  void await_resume() noexcept;
};
} // namespace coro
} // namespace folly

namespace other {
namespace folly {
namespace coro {
template <typename T> void blockingWait(::folly::coro::Task<T>);
} // namespace coro
} // namespace folly
} // namespace other

// a user awaitable that merely holds a task: awaiting it does not run the task here either
struct Wrapper {
  explicit Wrapper(folly::coro::Task<void> t);
  bool await_ready() noexcept;
  std::coroutine_handle<> await_suspend(std::coroutine_handle<>) noexcept;
  void await_resume() noexcept;
  folly::coro::Task<void> inner;
};

// an EAGER task type, deliberately not in the model list: its body runs at creation
template <typename T>
struct EagerTask {
  using promise_type = EagerTask<T>;
  bool await_ready() noexcept;
  std::coroutine_handle<> await_suspend(std::coroutine_handle<EagerTask<T>>) noexcept;
  void await_resume() noexcept;
  EagerTask<T> get_return_object();
  std::suspend_never initial_suspend() noexcept { return {}; }
  std::suspend_always final_suspend() noexcept { return {}; }
  void return_void();
  void unhandled_exception() {}
};

// Each shape gets its own mutex, inverted against the shared publish_ by its own partner, so
// every shape is its own canonical lock pair. A deadlock is deduplicated per file and canonical
// pair, so shapes sharing one pair collapse into a single row and a regression in any of them
// hides behind the others. The _ok shapes need their own pairs for the same reason, pointing the
// other way: if one of them started running the body here, its report would be merged into a
// reported shape's row and the suite would stay green.
//
// What the shapes are about is how the body is reached, so body(), eager_body() and
// takes_publish() stay shared: only the mutex taken around them differs.
namespace lazy_coroutine_creation {

class C {
 public:
  void takes_publish() { std::lock_guard<std::mutex> g(publish_); }

  // body locks publish_ via a callee, so no guard is live at co_return
  folly::coro::Task<void> body() {
    takes_publish();
    co_return;
  }

  EagerTask<void> eager_body() {
    takes_publish();
    co_return;
  }

  // the task is created in one function and awaited in another: the attribute cannot cross,
  // so this must stay conservative -- at worst behave as it did before the change
  folly::coro::Task<void> helper_returns_task() { return body(); }

  // ---- creating a lazy task under a lock does NOT run the body ------------------------------
  void partner_create() {
    std::lock_guard<std::mutex> g1(publish_);
    std::lock_guard<std::mutex> g2(create_);
  }

  void create_under_lock_ok() {
    std::lock_guard<std::mutex> g(create_);
    folly::coro::Task<void> t = body();
  }

  // ---- awaiting it inline DOES run it here ---------------------------------------------------
  void partner_await_bad() {
    std::lock_guard<std::mutex> g1(publish_);
    std::lock_guard<std::mutex> g2(await_);
  }

  folly::coro::Task<void> await_under_lock_bad() {
    {
      std::lock_guard<std::mutex> g(await_);
      co_await body();
    }
    co_return;
  }

  // ---- blockingWait runs it on this thread ---------------------------------------------------
  void partner_blocking_wait_bad() {
    std::lock_guard<std::mutex> g1(publish_);
    std::lock_guard<std::mutex> g2(blocking_wait_);
  }

  void blocking_wait_under_lock_bad() {
    std::lock_guard<std::mutex> g(blocking_wait_);
    folly::coro::blockingWait(body());
  }

  // ---- an eager coroutine really does run at creation ----------------------------------------
  void partner_eager_bad() {
    std::lock_guard<std::mutex> g1(publish_);
    std::lock_guard<std::mutex> g2(eager_);
  }

  void create_eager_under_lock_bad() {
    std::lock_guard<std::mutex> g(eager_);
    EagerTask<void> t = eager_body();
  }

  // ---- created in one procedure, awaited in another ------------------------------------------
  void partner_cross_procedure_bad() {
    std::lock_guard<std::mutex> g1(publish_);
    std::lock_guard<std::mutex> g2(cross_procedure_);
  }

  folly::coro::Task<void> await_cross_procedure_bad() {
    {
      std::lock_guard<std::mutex> g(cross_procedure_);
      co_await helper_returns_task();
    }
    co_return;
  }

  // ---- blockingWait of a task held in a local, the ordinary folly spelling: the body runs on
  //      this thread, so the lock order is this frame's ------------------------------------------
  void partner_moved_local_bad() {
    std::lock_guard<std::mutex> g1(publish_);
    std::lock_guard<std::mutex> g2(moved_local_);
  }

  void blocking_wait_moved_local_bad() {
    auto t = body();
    std::lock_guard<std::mutex> g(moved_local_);
    folly::coro::blockingWait(std::move(t));
  }

  void partner_local_bad() {
    std::lock_guard<std::mutex> g1(publish_);
    std::lock_guard<std::mutex> g2(local_);
  }

  void blocking_wait_local_bad() {
    auto t = body();
    std::lock_guard<std::mutex> g(local_);
    folly::coro::blockingWait(t);
  }

  // ---- reached through a reference to the local: blockingWait still runs the body here --------
  void partner_through_reference_bad() {
    std::lock_guard<std::mutex> g1(publish_);
    std::lock_guard<std::mutex> g2(through_reference_);
  }

  void blocking_wait_through_reference_bad() {
    auto t = body();
    folly::coro::Task<void>& r = t;
    std::lock_guard<std::mutex> g(through_reference_);
    folly::coro::blockingWait(std::move(r));
  }

  // ---- not folly::coro::blockingWait, so it does not run the body here: this reported while
  //      the name test was a substring of the printed procedure name ---------------------------
  void partner_async() {
    std::lock_guard<std::mutex> g1(publish_);
    std::lock_guard<std::mutex> g2(async_);
  }

  void blocking_wait_async_ok() {
    std::lock_guard<std::mutex> g(async_);
    folly::coro::blockingWaitAsync(body());
  }

  // ---- a lookalike in another namespace: this one DID report before the matcher became exact --
  void partner_lookalike() {
    std::lock_guard<std::mutex> g1(publish_);
    std::lock_guard<std::mutex> g2(lookalike_);
  }

  void lookalike_blocking_wait_ok() {
    std::lock_guard<std::mutex> g(lookalike_);
    other::folly::coro::blockingWait(body());
  }

  // ---- awaiting a task bound to an executor does not run its body here, so the lock order of
  //      the body is not this frame's -----------------------------------------------------------
  void partner_with_executor() {
    std::lock_guard<std::mutex> g1(publish_);
    std::lock_guard<std::mutex> g2(with_executor_);
  }

  folly::coro::Task<void> await_task_with_executor_ok() {
    auto t = body();
    folly::coro::TaskWithExecutor<void> scheduled(std::move(t));
    {
      std::lock_guard<std::mutex> g(with_executor_);
      co_await scheduled;
    }
    co_return;
  }

  // ---- the same for a user-written awaitable that merely holds a task -------------------------
  void partner_user_wrapper() {
    std::lock_guard<std::mutex> g1(publish_);
    std::lock_guard<std::mutex> g2(user_wrapper_);
  }

  folly::coro::Task<void> await_user_wrapper_ok() {
    auto t = body();
    Wrapper w(std::move(t));
    {
      std::lock_guard<std::mutex> g(user_wrapper_);
      co_await w;
    }
    co_return;
  }

  // ---- a plain synchronous call must still report ---------------------------------------------
  void partner_sync_bad() {
    std::lock_guard<std::mutex> g1(publish_);
    std::lock_guard<std::mutex> g2(sync_);
  }

  void sync_under_lock_bad() {
    std::lock_guard<std::mutex> g(sync_);
    takes_publish();
  }

 private:
  std::mutex publish_;
  std::mutex create_;
  std::mutex await_;
  std::mutex blocking_wait_;
  std::mutex eager_;
  std::mutex cross_procedure_;
  std::mutex moved_local_;
  std::mutex local_;
  std::mutex through_reference_;
  std::mutex async_;
  std::mutex lookalike_;
  std::mutex with_executor_;
  std::mutex user_wrapper_;
  std::mutex sync_;
};

} // namespace lazy_coroutine_creation
