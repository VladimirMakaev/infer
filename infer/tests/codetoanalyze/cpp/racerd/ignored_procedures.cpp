/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <atomic>
#include <mutex>

namespace ignored_procedures {

// A handler called from everywhere, which records the first failure under a
// try_lock and publishes it with a release store. Its accesses do not race with
// the callers, but every caller inherits them, so an unrelated class that merely
// uses a lock and the macro is reported.
//
// --racerd-ignore-procedures makes a call to the handler contribute nothing. This
// directory does not pass the option, so the reports below are the DEFAULT
// behaviour and must not change; the option is exercised separately.
struct AssertContext {
  int line = 0;
  bool populated = false;
};

AssertContext gCtx;
std::mutex gAssertLock;
std::atomic<bool> gFired{false};

void readAfterAcquire() {
  if (gFired.load(std::memory_order_acquire) && gCtx.populated) {
  }
}

void handleAssert(int line) {
  if (gAssertLock.try_lock()) {
    if (!gCtx.populated) {
      gCtx.line = line;
      gCtx.populated = true;
    }
    gFired.store(true, std::memory_order_release);
    gAssertLock.unlock();
  }
  readAfterAcquire();
}

#define IP_ASSERT(c)         \
  do {                       \
    if (!(c))                \
      handleAssert(__LINE__); \
  } while (0)

// An unrelated class that happens to use a lock and the assert macro. Reported by
// default because the handler's accesses are inherited here; silent when the
// handler is named by --racerd-ignore-procedures.
class Counter {
 public:
  void set_bad(int v) {
    std::lock_guard<std::mutex> g(mu_);
    IP_ASSERT(v >= 0);
    value_ = v;
  }

  int get_bad() {
    IP_ASSERT(true);
    return value_;
  }

 private:
  std::mutex mu_;
  int value_ = 0;
};

// CONTROL: a genuine race on the class's own field, with no handler involved. It
// must keep being reported whether or not the handler is ignored.
class PlainRace {
 public:
  void set_bad(int v) {
    std::lock_guard<std::mutex> g(mu_);
    value_ = v;
  }

  int get_bad() { return value_; }

 private:
  std::mutex mu_;
  int value_ = 0;
};

} // namespace ignored_procedures
