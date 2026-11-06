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
// Eight of the nine fatal families now report PULSE_FATAL_ON_NULLABLE_RETURN. That issue type is
// disabled by default, so the Makefile turns it on with the existing --enable-issue-type; a build
// without that flag sees only the two NULLPTR_DEREFERENCE controls.
//
// The ninth keeps its FN_ marker and its reason is recorded at the function itself. A silence in
// this file is either a named _ok case or that one FN_, never an unexplained gap.
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

int invariant_msg_bad(DesktopView& v) {
  PanelAppHost* host = v.GetPanelAppHost();
  INVARIANT_MSG(host != nullptr, "no PanelAppHost");
  return host->id;
}

// Helper-mediated: the check lives in the macro's lambda, a procedure of its own.
int precondition_notnull_bad(mr::VulkanContextProvider* p) {
  Rhi* rhi = PRECONDITION_NOTNULL(p->rhi());
  return rhi->x;
}

int check_notnull_bad(mr::VulkanContextProvider* p) {
  Rhi* rhi = p->rhi();
  CHECK_NOTNULL(rhi);
  return rhi->x;
}

int glog_check_bad(mr::VulkanContextProvider* p) {
  Rhi* rhi = p->rhi();
  GLOG_CHECK(rhi != nullptr);
  return rhi->x;
}

int production_assert_bad(mr::VulkanContextProvider* p) {
  Rhi* rhi = p->rhi();
  PRODUCTION_ASSERT(rhi);
  return rhi->x;
}

int log_always_fatal_if_bad(mr::VulkanContextProvider* p) {
  Rhi* rhi = p->rhi();
  LOG_ALWAYS_FATAL_IF(rhi == nullptr, "no rhi");
  return rhi->x;
}

int bionic_assert_bad(mr::VulkanContextProvider* p) {
  Rhi* rhi = p->rhi();
  bionic_assert(rhi != nullptr);
  return rhi->x;
}

// OUT OF SCOPE, deliberately, and kept so that the other eight are not mistaken for the whole
// story. The clang frontend deletes this branch before Pulse ever sees it: cTrans_utils
// trans_assertion is `if Config.report_custom_error then trans_assertion_failure else
// trans_assume_false`, so by default the assert-failure branch is assumed false (measured: 0
// files mention __infer_fail by default, 6 with --report-custom-error).
//
// Making it reachable is NOT merely a flag away, which is why this is an FN_ and not a TODO:
// turning the branch back on revives a documented UNNECESSARY_COPY false positive through the
// non-disjunctive join. Fixing that is a separate piece of work, tracked as
// pulse-nondisj-join-fatal-exit-as-bottom; until it lands, reaching this row would cost an
// existing true report elsewhere.
int FN_glibc_assert_bad(mr::VulkanContextProvider* p) {
  Rhi* rhi = p->rhi();
  glibc_assert(rhi);
  return rhi->x;
}

// Helper-mediated: the null value crosses one call before it reaches the check.
static void requireRhi(Rhi* rhi) { CHECK_NOTNULL(rhi); }

int fatal_in_callee_bad(mr::VulkanContextProvider* p) {
  Rhi* rhi = p->rhi();
  requireRhi(rhi);
  return rhi->x;
}

// --- one fixture per precision clause -------------------------------------------------------
// Without these the rule's three conditions are untested: mutating any of them out left the
// suite green and the expectation file byte-identical, because the cases above are all rejected
// by the first condition alone.

// TWO independent failure values, each with its own check. Both report, and each names its own
// call. This is what pins "can this crash site be reached with the value non-null": on the path
// where both calls failed, BOTH origins are null at the first check, so a rule that did not ask
// the question per crash site would blame the first check for the second call's failure too.
int two_failures_bad(mr::VulkanContextProvider* p, DesktopView& v) {
  PanelAppHost* host = v.GetPanelAppHost();
  Rhi* rhi = p->rhi();
  CHECK_NOTNULL(host);
  CHECK_NOTNULL(rhi);
  return rhi->x + host->id;
}

// Dies on every path, so the null return is not what kills it. Measured to be rejected by "some
// path survives" ALONE: its check-site exit is manifest and does carry the origin, so the other
// two conditions are both satisfied here.
void always_fatal_ok(mr::VulkanContextProvider* p) {
  Rhi* rhi = p->rhi();
  CHECK_NOTNULL(rhi);
  ::applog::logFatal("unconditional");
}

// Silent because of the manifest-only condition, and worth keeping because the reason is a real
// limitation rather than a design choice. Reaching the second check requires mode >= 0, a
// constraint on a parameter, and Pulse classifies the resulting state as not manifest -- so the
// crash this rule exists to report is suppressed when the path to it also constrains an integer
// parameter. Measured, not assumed: that exit reports manifest=false while the mode < 0 exits
// above it report manifest=true.
int mode_dependent_fatal_ok(mr::VulkanContextProvider* p, int mode) {
  Rhi* rhi = p->rhi();
  GLOG_CHECK(mode >= 0);
  CHECK_NOTNULL(rhi);
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
