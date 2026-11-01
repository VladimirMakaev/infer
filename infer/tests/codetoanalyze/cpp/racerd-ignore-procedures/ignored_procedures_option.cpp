/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Run with --racerd-ignore-procedures 'ignore_procs::handleAssert' (see the Makefile).
// cpp/racerd/ignored_procedures.cpp pins the same shapes WITHOUT the option.

#include <mutex>

namespace ignore_procs {

struct AssertContext {
  int line = 0;
  bool populated = false;
};

AssertContext gCtx;
std::mutex gAssertLock;

void readContext() {
  if (gCtx.populated) {
  }
}

// Matched EXACTLY by the regex: a call to it contributes nothing.
void handleAssert(int line) {
  if (gAssertLock.try_lock()) {
    if (!gCtx.populated) {
      gCtx.line = line;
      gCtx.populated = true;
    }
    gAssertLock.unlock();
  }
  readContext();
}

// Matched by PREFIX: the regex is anchored at the start only, so a longer name
// that begins with it is ignored too.
void handleAssertion(int line) {
  if (gAssertLock.try_lock()) {
    if (!gCtx.populated) {
      gCtx.line = line;
      gCtx.populated = true;
    }
    gAssertLock.unlock();
  }
  readContext();
}

// The handler's accesses are not inherited, so this class is silent.
class UsesHandler {
 public:
  void set_ok(int v) {
    std::lock_guard<std::mutex> g(mu_);
    handleAssert(__LINE__);
    value_ = v;
  }

  int get_ok() {
    handleAssert(__LINE__);
    return 0;
  }

 private:
  std::mutex mu_;
  int value_ = 0;
};

// Ignored by the same regex through prefix matching.
class UsesPrefixHandler {
 public:
  void set_ok(int v) {
    std::lock_guard<std::mutex> g(mu_);
    handleAssertion(__LINE__);
    value_ = v;
  }

  int get_ok() {
    handleAssertion(__LINE__);
    return 0;
  }

 private:
  std::mutex mu_;
  int value_ = 0;
};

// THE HAZARD the help text warns about. This helper is a free function whose
// qualified name starts with the regex, so it is ignored AND it acquires a
// lock. Ignoring it drops that acquisition: the read below is protected only
// through it while the write is locked inline, so the read is reported.
// Naming a lock-acquiring procedure costs you a report that did not exist
// before the option was used.
std::mutex gMixedMu;

void handleAssertLockUp() { gMixedMu.lock(); }
void mixedUnlock() { gMixedMu.unlock(); }

class MixedLocking {
 public:
  void set_direct(int v) {
    std::lock_guard<std::mutex> g(gMixedMu);
    value_ = v;
  }

  int get_via_ignored_helper_bad() {
    handleAssertLockUp();
    int v = value_;
    mixedUnlock();
    return v;
  }

 private:
  int value_ = 0;
};

// CONTROL: a genuine race with no handler involved. The option must not hide it.
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

} // namespace ignore_procs

// CONTROL for qualified-name matching: the regex is `ignore_procs::handleAssert`,
// so this global-scope handler of the same bare name is NOT ignored, and the
// class that calls it keeps inheriting its accesses.
::ignore_procs::AssertContext gGlobalCtx;
std::mutex gGlobalLock;

void globalReadContext() {
  if (gGlobalCtx.populated) {
  }
}

void handleAssert(int line) {
  if (gGlobalLock.try_lock()) {
    if (!gGlobalCtx.populated) {
      gGlobalCtx.line = line;
      gGlobalCtx.populated = true;
    }
    gGlobalLock.unlock();
  }
  globalReadContext();
}

class UsesUnqualifiedHandler {
 public:
  void set_bad(int v) {
    std::lock_guard<std::mutex> g(mu_);
    ::handleAssert(__LINE__);
    value_ = v;
  }

  int get_bad() {
    ::handleAssert(__LINE__);
    return 0;
  }

 private:
  std::mutex mu_;
  int value_ = 0;
};
