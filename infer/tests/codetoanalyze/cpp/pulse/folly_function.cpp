/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <utility>

// Minimal folly::Function caricature for testing Pulse models
namespace folly {

template <class FunctionType>
class Function;

namespace detail {
namespace function {
template <class FunctionType>
struct FunctionTraits;

template <class ReturnType, class... Args>
struct FunctionTraits<ReturnType(Args...)> {
  ReturnType operator()(Args...);
};
} // namespace function
} // namespace detail

using Exec = std::size_t (*)(int, void*, void*);

struct Data {
  void* object_;
};

template <class ReturnType, class... Args>
class Function<ReturnType(Args...)>
    : private detail::function::FunctionTraits<ReturnType(Args...)> {
 public:
  Function() noexcept : exec_(nullptr) {}

  Function(Function&& that) noexcept
      : call_(that.call_), exec_(that.exec_) {
    that.exec_ = nullptr;
  }

  template <class Fun>
  Function(Fun&& fun) : exec_(reinterpret_cast<Exec>(1)) {}

  // Mirrors real folly::Function::operator= which calls exec() helper
  Function& operator=(Function&& that) noexcept {
    destroyCurrent();
    call_ = that.call_;
    exec_ = that.exec_;
    that.exec_ = nullptr;
    return *this;
  }

  template <class Fun>
  Function& operator=(Fun&& fun) {
    destroyCurrent();
    exec_ = reinterpret_cast<Exec>(1);
    return *this;
  }

  ~Function() { destroyCurrent(); }

  explicit operator bool() const noexcept { return exec_ != nullptr; }

  using detail::function::FunctionTraits<ReturnType(Args...)>::operator();

 private:
  // Mirrors folly::Function::exec — reads exec_ to dispatch cleanup.
  // This indirection is where Infer loses track of exec_ initialization.
  void exec(int op, Data* src, Data* dst) {
    if (!exec_) {
      return;
    }
    exec_(op, src, dst);
  }

  void destroyCurrent() {
    exec(0, &data_, nullptr);
  }

  // Public only so a test can read one of these the way the frontend's
  // field-by-field copy of a `?:` temporary already does. Pulse does not model
  // access control, so this changes nothing about the analysis.
 public:
  ReturnType (*call_)(Data&, Args&&...){nullptr};
  Exec exec_{nullptr};
  Data data_{};
};

} // namespace folly

enum class JobExitCode { Done, Reschedule };
using Job = folly::Function<JobExitCode()>;

struct QueueEntry {
  Job job;
  int rescheduleCount{};

  QueueEntry(Job j) : job{std::move(j)} {}
  QueueEntry(QueueEntry&&) = default;
  QueueEntry& operator=(QueueEntry&&) = default;
};

// Reproduces the FP from cachelib's ThreadPoolJobQueue: after move-constructing
// a QueueEntry, assigning Job{} to entry.job via operator= was reported as
// reading an uninitialized value (exec_ inside folly::Function).
void folly_function_move_assign_ok() {
  QueueEntry entry{Job{[] { return JobExitCode::Done; }}};
  // This operator= call was previously a FP (PULSE_UNINITIALIZED_VALUE)
  // because Infer had no model for folly::Function::operator=.
  entry.job = Job{};
}

void folly_function_construct_and_call_ok() {
  folly::Function<int()> f{[] { return 42; }};
  int x = f();
}

void folly_function_move_construct_ok() {
  folly::Function<int()> f1{[] { return 42; }};
  folly::Function<int()> f2{std::move(f1)};
  int x = f2();
}

void folly_function_move_assign_call_ok() {
  folly::Function<int()> f1{[] { return 42; }};
  folly::Function<int()> f2;
  f2 = std::move(f1);
  int x = f2();
}

// Reproduces the FP from folly::CPUThreadPoolExecutor::add and
// folly::settings::CommandLineParser::Impl::parse: operator bool() reads
// exec_ which Infer thinks is uninitialized after move construction.
void folly_function_operator_bool_ok() {
  QueueEntry entry{Job{[] { return JobExitCode::Done; }}};
  if (entry.job) {
    entry.job();
  }
}

// Reproduces the FPs from a diff-time report on three more shapes: an
// aggregate with Function members moved into a `?:` temporary, a Function
// built from a lambda in one `?:` branch, and a direct field read after the
// converting constructor. The model for the folly::Function
// constructor and operator= writes the Function as one opaque value and never
// writes call_/exec_/data_, so anything that reads a field from outside a
// modelled method sees it as uninitialized. The existing cases above do not
// catch this because every method that reads a field -- operator bool,
// operator= -- is itself modelled.

struct TwoFunctions {
  folly::Function<int()> first{};
  folly::Function<int()> second{};
};

void sinkTwoFunctions(TwoFunctions t);
void sinkFunction(folly::Function<int()> f);
folly::Function<int()> functionFromLambda() {
  return [] { return 42; };
}

// A `?:` whose branches are aggregates with Function members: the implicit
// move constructor of the aggregate moves each member, and the frontend copies
// the branch temporary into SIL_temp_conditional field by field.
void folly_function_cond_aggregate_move_ok(bool c, TwoFunctions* p) {
  sinkTwoFunctions(c ? TwoFunctions{} : static_cast<TwoFunctions&&>(*p));
}

// A `?:` with a Function built from a lambda in one branch.
void folly_function_cond_from_lambda_ok(bool c, folly::Function<int()> r) {
  sinkFunction(c ? static_cast<folly::Function<int()>&&>(r)
                 : functionFromLambda());
}

// No `?:` at all: read a field directly after the converting constructor.
bool folly_function_field_after_converting_ctor_ok() {
  folly::Function<int()> f = functionFromLambda();
  return f.exec_ != nullptr;
}
