/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// A CHECK / INVARIANT / ASSERT on a value returned by a may-return-null function aborts on the
// null branch. Pulse sees the abort -- the disjunct is an ExitProgram -- but nothing reports it,
// so the crash is invisible while the unchecked dereference one line away is a
// NULLPTR_DEREFERENCE. The two control functions at the bottom are that dereference, and they are
// what proves the nullable list below is actually matching.
//
// The fatal functions are named FN_ because no slice of this work has landed yet: today every one
// of them is silent. They are not a list of limitations, they are the acceptance target -- each
// commit in the chain flips specific names here, and issues.exp records what is true at that
// commit rather than what is intended.
//
// Nullable returns come from --pulse-model-return-nullable-list in the Makefile, which is how the
// generated list will supply them. Both listed functions are reductions of real crashes:
// mr::VulkanContextProvider::rhi() returns nullptr after logging that OpenXR Vulkan state is
// unavailable, and DesktopView::GetPanelAppHost() is the getter an INVARIANT_MSG guards.

#include <cstddef>
#include <cstdlib>
#include <utility>

struct Rhi {
  int x;
};
struct PanelAppHost {
  int id;
};

namespace mr {
struct VulkanContextProvider {
  Rhi* rhi();
};
} // namespace mr

struct DesktopView {
  PanelAppHost* GetPanelAppHost();
};

// --- the fatal families, reduced from their real headers ------------------------------------

// Log, then abort(): abort() is already a Pulse model.
void LogErrorWithPrefix(const char* file, const char* fmt, ...);
#define INVARIANT_MSG(cond, msg)             \
  {                                          \
    if (!(cond)) {                           \
      LogErrorWithPrefix(__FILE__, msg);     \
      abort();                               \
    }                                        \
  }

// A project's own [[noreturn]] fatal reporter, the tail of a CHECK-style macro. Infer knows no
// such name and must not: being [[noreturn]] and not an orderly exit is what makes it fatal.
namespace applog {
[[noreturn]] void logFatal(const char* msg) noexcept;
}
#define CHECK_NOTNULL(v) \
  ((v) != nullptr ? (void)0 : ::applog::logFatal(#v " is null"))

// The shipped precondition macro wraps the check in a LAMBDA, so the check is a separate
// procedure and the constraint is on the POINTEE of a reference parameter, not on a formal.
#define PRECONDITION_NOTNULL(v)                    \
  ([](auto&& g) -> decltype(g) {                   \
    CHECK_NOTNULL(g);                              \
    return std::forward<decltype(g)>(g);           \
  }(v))

// A temporary whose [[noreturn]] DESTRUCTOR aborts.
namespace google {
struct LogMessageFatal {
  LogMessageFatal(const char* f, int l);
  [[noreturn]] ~LogMessageFatal();
};
} // namespace google
#define GLOG_CHECK(c) \
  if (!(c))      \
  google::LogMessageFatal(__FILE__, __LINE__)

// handleAssert is NOT [[noreturn]]; it is reached through --pulse-model-abort in the Makefile,
// the same way the real repo's analyze config lists it.
namespace assertions {
struct AssertHandler {
  static void handleAssert(const char* expr, const char* file, int line);
};
} // namespace assertions
#define PRODUCTION_ASSERT(x)                                      \
  do {                                                            \
    if (!(x))                                                     \
      assertions::AssertHandler::handleAssert(#x, __FILE__, __LINE__); \
  } while (0)

extern "C" void __android_log_assert(const char* cond,
                                     const char* tag,
                                     const char* fmt,
                                     ...) __attribute__((__noreturn__));
#define LOG_ALWAYS_FATAL_IF(cond, msg)              \
  ((__builtin_expect((cond) != 0, 0))               \
       ? ((void)__android_log_assert(#cond, "tag", msg)) \
       : ((void)0))

extern "C" void __assert2(const char* file,
                          int line,
                          const char* function,
                          const char* msg) __attribute__((__noreturn__));
#define bionic_assert(e) \
  ((e) ? (void)0 : __assert2(__FILE__, __LINE__, __PRETTY_FUNCTION__, #e))

extern "C" void __assert_fail(const char* assertion,
                              const char* file,
                              unsigned int line,
                              const char* function) __attribute__((__noreturn__));
#define glibc_assert(e) \
  ((e) ? (void)0 : __assert_fail(#e, __FILE__, __LINE__, __PRETTY_FUNCTION__))

// --- one fatal family each: all silent today ------------------------------------------------

int FN_invariant_msg_bad(DesktopView& v) {
  PanelAppHost* host = v.GetPanelAppHost();
  INVARIANT_MSG(host != nullptr, "no PanelAppHost");
  return host->id;
}

// Helper-mediated: the check lives in the macro's lambda, a procedure of its own.
int FN_precondition_notnull_bad(mr::VulkanContextProvider* p) {
  Rhi* rhi = PRECONDITION_NOTNULL(p->rhi());
  return rhi->x;
}

int FN_check_notnull_bad(mr::VulkanContextProvider* p) {
  Rhi* rhi = p->rhi();
  CHECK_NOTNULL(rhi);
  return rhi->x;
}

int FN_glog_check_bad(mr::VulkanContextProvider* p) {
  Rhi* rhi = p->rhi();
  GLOG_CHECK(rhi != nullptr);
  return rhi->x;
}

int FN_production_assert_bad(mr::VulkanContextProvider* p) {
  Rhi* rhi = p->rhi();
  PRODUCTION_ASSERT(rhi);
  return rhi->x;
}

int FN_log_always_fatal_if_bad(mr::VulkanContextProvider* p) {
  Rhi* rhi = p->rhi();
  LOG_ALWAYS_FATAL_IF(rhi == nullptr, "no rhi");
  return rhi->x;
}

int FN_bionic_assert_bad(mr::VulkanContextProvider* p) {
  Rhi* rhi = p->rhi();
  bionic_assert(rhi != nullptr);
  return rhi->x;
}

// The one family that needs TWO things that are not in this suite, both measured:
//   1. the frontend deletes it. cTrans_utils trans_assertion is
//      `if Config.report_custom_error then trans_assertion_failure else trans_assume_false`, so
//      by default the assert-failure branch is assumed false and gone before Pulse runs --
//      __infer_fail is not even emitted (measured: 0 files mention it by default, 6 with
//      --report-custom-error).
//   2. --report-custom-error alone is still not enough: it emits a call to __infer_fail, which
//      needs a Pulse model before it becomes an exit at all.
// So this case stays silent until both land, and it is kept rather than dropped because a suite
// that quietly omitted the one family the frontend removes would make the other eight look like
// the whole story.
int FN_glibc_assert_bad(mr::VulkanContextProvider* p) {
  Rhi* rhi = p->rhi();
  glibc_assert(rhi);
  return rhi->x;
}

// Helper-mediated: the null value crosses one call before it reaches the check.
static void requireRhi(Rhi* rhi) { CHECK_NOTNULL(rhi); }

int FN_fatal_in_callee_bad(mr::VulkanContextProvider* p) {
  Rhi* rhi = p->rhi();
  requireRhi(rhi);
  return rhi->x;
}

// --- must stay silent -------------------------------------------------------------------------

// Handles null instead of aborting: this is the shape a fix turns the _bad cases into, so a rule
// that reported here would be reporting the fix.
int graceful_ok(mr::VulkanContextProvider* p) {
  Rhi* rhi = p->rhi();
  if (rhi == nullptr) {
    return -1;
  }
  return rhi->x;
}

// Two fatals, neither decided by the nullable value: the first is unreachable because null is
// already excluded, the second is about an unrelated argument. This is the case that separates
// "a fatal is on this path" from "the nullable return decided this fatal".
int unrelated_fatal_ok(mr::VulkanContextProvider* p, int mode) {
  Rhi* rhi = p->rhi();
  if (rhi == nullptr) {
    return -1;
  }
  CHECK_NOTNULL(rhi);
  GLOG_CHECK(mode >= 0);
  return rhi->x + mode;
}

// Not on the nullable list, so its contract is unknown and a CHECK on it says nothing.
Rhi* unlisted();

int unlisted_ok() {
  Rhi* rhi = unlisted();
  GLOG_CHECK(rhi != nullptr);
  return rhi->x;
}

// --- TP controls: these DO report today -------------------------------------------------------
// If the nullable list ever stops matching, these two go silent and the whole suite becomes
// vacuous without a single FN_ name changing. They are the reason the suite is not just a list
// of silences.

int unchecked_deref_control_bad(DesktopView& v) {
  return v.GetPanelAppHost()->id;
}

int unchecked_deref_control2_bad(mr::VulkanContextProvider* p) {
  return p->rhi()->x;
}
