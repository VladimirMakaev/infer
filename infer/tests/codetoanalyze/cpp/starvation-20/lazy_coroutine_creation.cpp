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

namespace lazy_coroutine_creation {

class C {
 public:
  // the real order everywhere else
  void publish_then_m_bad() {
    std::lock_guard<std::mutex> g1(publish_);
    std::lock_guard<std::mutex> g2(m_);
  }

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

  // creating the lazy task under m_ does NOT run the body
  void create_under_lock_ok() {
    std::lock_guard<std::mutex> g(m_);
    folly::coro::Task<void> t = body();
  }

  // awaiting it inline DOES run it here
  folly::coro::Task<void> await_under_lock_bad() {
    {
      std::lock_guard<std::mutex> g(m_);
      co_await body();
    }
    co_return;
  }

  // blockingWait runs it on this thread
  void blocking_wait_under_lock_bad() {
    std::lock_guard<std::mutex> g(m_);
    folly::coro::blockingWait(body());
  }

  // an eager coroutine really does run at creation
  void create_eager_under_lock_bad() {
    std::lock_guard<std::mutex> g(m_);
    EagerTask<void> t = eager_body();
  }

  // the task is created in one function and awaited in another: the attribute cannot cross,
  // so this must stay conservative -- at worst behave as it did before the change
  folly::coro::Task<void> helper_returns_task() { return body(); }

  folly::coro::Task<void> await_cross_procedure_bad() {
    {
      std::lock_guard<std::mutex> g(m_);
      co_await helper_returns_task();
    }
    co_return;
  }

  // blockingWait of a task held in a local, the ordinary folly spelling: the body runs on
  // this thread, so the lock order is this frame's
  void blocking_wait_moved_local_bad() {
    auto t = body();
    std::lock_guard<std::mutex> g(m_);
    folly::coro::blockingWait(std::move(t));
  }

  void blocking_wait_local_bad() {
    auto t = body();
    std::lock_guard<std::mutex> g(m_);
    folly::coro::blockingWait(t);
  }

  // not folly::coro::blockingWait, so it does not run the body here: this reported while the
  // name test was a substring of the printed procedure name
  void blocking_wait_async_ok() {
    std::lock_guard<std::mutex> g(m_);
    folly::coro::blockingWaitAsync(body());
  }

  // a lookalike in another namespace: this one DID report before the matcher became exact
  void lookalike_blocking_wait_ok() {
    std::lock_guard<std::mutex> g(m_);
    other::folly::coro::blockingWait(body());
  }

  // awaiting a task bound to an executor does not run its body here, so the lock order of
  // the body is not this frame's
  folly::coro::Task<void> await_task_with_executor_ok() {
    auto t = body();
    folly::coro::TaskWithExecutor<void> scheduled(std::move(t));
    {
      std::lock_guard<std::mutex> g(m_);
      co_await scheduled;
    }
    co_return;
  }

  // the same for a user-written awaitable that merely holds a task
  folly::coro::Task<void> await_user_wrapper_ok() {
    auto t = body();
    Wrapper w(std::move(t));
    {
      std::lock_guard<std::mutex> g(m_);
      co_await w;
    }
    co_return;
  }

  // reached through a reference to the local: blockingWait still runs the body here
  void blocking_wait_through_reference_bad() {
    auto t = body();
    folly::coro::Task<void>& r = t;
    std::lock_guard<std::mutex> g(m_);
    folly::coro::blockingWait(std::move(r));
  }

  // a plain synchronous call must still report
  void sync_under_lock_bad() {
    std::lock_guard<std::mutex> g(m_);
    takes_publish();
  }

 private:
  std::mutex m_;
  std::mutex publish_;
};

} // namespace lazy_coroutine_creation
