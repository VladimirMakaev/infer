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
// The ninth keeps its FN_ marker and its reason is recorded at the function itself. A second FN_
// is the priced cost of the handled-branch clause, recorded the same way. A silence in this file
// is either a named _ok case or one of those two FN_s, never an unexplained gap.
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
// Without these the rule's four conditions are untested: mutating any of them out left the
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
//
// Two exits, two reasons, and only the first was written down here originally. The mode < 0
// exits ARE manifest and do carry the origin, and what rejects them is "no fatal exit happens
// WITHOUT it": the same GLOG_CHECK site is reached with rhi non-null too. Mutating that clause
// out makes this function report at `google::LogMessageFatal::~LogMessageFatal`, not at the
// CHECK_NOTNULL, which is how the second reason was found.
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
//
// It is no longer a test of any single clause, and shared_fatal_site_ok exists because of that:
// the early return makes this a handled branch too, so dropping either clause alone leaves the
// other one rejecting it. Measured -- it reports under none of the three single-clause
// mutations. Kept as the shape it documents, not relied on as a discriminator.
int unrelated_fatal_ok(mr::VulkanContextProvider* p, int mode) {
  Rhi* rhi = p->rhi();
  if (rhi == nullptr) {
    return -1;
  }
  CHECK_NOTNULL(rhi);
  GLOG_CHECK(mode >= 0);
  return rhi->x + mode;
}

// --- the null branch is HANDLED, and a callee on it can still not return ----------------------
// These two are the most common false positive this rule produces on real code: in one measured
// project 133 of 239 locations ended in a throwing library helper, and 16 of 16 sampled were
// wrong. The null branch does the
// right thing -- logs and returns, or creates the missing object -- but a callee on that branch
// has its own [[noreturn]] error path whose condition has nothing to do with the null. That exit
// is reachable only under the null, because the branch is the only place the call sits, and
// before the fix the rule read "reachable only under the null" as "caused by the null".
//
// Both reductions keep the property that matters: the throwing helper is a DECLARATION, so Pulse
// cannot see that its guard is unrelated, which is exactly the real situation with fmt and absl.

// fmt::vformat: the error path is taken only for a malformed format string.
namespace fmtlike {
[[noreturn]] void throw_format_error(const char* msg);
int parse_spec(const char* s);
inline void log_error(const char* fmt) {
  if (parse_spec(fmt) < 0) {
    throw_format_error("invalid format string");
  }
}
} // namespace fmtlike

int graceful_log_throws_ok(mr::VulkanContextProvider* p) {
  Rhi* rhi = p->rhi();
  if (rhi == nullptr) {
    fmtlike::log_error("no rhi");
    return -1;
  }
  return rhi->x;
}

// A registry's findOrCreate: find() null -> create(), whose container member has a length check
// that ends in absl::ThrowStdLengthError.
[[noreturn]] void ThrowStdLengthError(const char* what);
std::size_t requested_size();
// Hands back a pointer it does not own: the real one allocates, but an allocation here would add
// a MEMORY_LEAK_CPP row that has nothing to do with what this fixture is for.
Rhi* registeredRhi();
inline Rhi* makeRhi() {
  if (requested_size() > static_cast<std::size_t>(-1) / 2) {
    ThrowStdLengthError("length check");
  }
  return registeredRhi();
}

int find_or_create_throws_ok(mr::VulkanContextProvider* p) {
  Rhi* rhi = p->rhi();
  if (rhi != nullptr) {
    return rhi->x;
  }
  return makeRhi()->x;
}

// The cost of the clause above, stated rather than discovered later. The null branch really does
// crash, on a second condition that really is about the failure -- and it also recovers. Nothing
// available here separates that from the two cases above: both are "the null branch splits, one
// side dies". The field sample says the logger shape is the overwhelming majority, so the clause
// is worth this row.
bool canRecover();
int FN_recoverable_then_fatal_bad(mr::VulkanContextProvider* p) {
  Rhi* rhi = p->rhi();
  if (rhi == nullptr) {
    if (canRecover()) {
      return -1;
    }
    ::applog::logFatal("no rhi and no fallback");
  }
  return rhi->x;
}

// TWO failure values, one handled and one fatal, both obtained BEFORE either is checked. The
// path that returns because `host` is null also carries `rhi` as null, because rhi's check is
// never reached on that path and so nothing constrains it. A handled-branch test phrased over
// the whole procedure puts rhi in the handled set and loses this report -- silently, and the
// more calls the nullable list matches the more often it happens. Found by the verifier, not
// by me: my first version of that clause was a union over every surviving path.
int other_origin_handled_bad(mr::VulkanContextProvider* p, DesktopView& v) {
  Rhi* rhi = p->rhi();
  PanelAppHost* host = v.GetPanelAppHost();
  if (host == nullptr) {
    return -1;
  }
  CHECK_NOTNULL(rhi);
  return rhi->x + host->id;
}

// A crash that needs BOTH failures at once, and the fixture that pins the handled-set match to
// equality rather than containment. The fatal pins {rhi, host}; the path that survives it pins
// only {rhi}, because it reached the inner check with host non-null and then returned through a
// helper that can throw. Containment would read that surviving path as "rhi was handled" and
// report neither value, losing the whole crash; equality asks for a surviving path that failed
// the same way, finds none, and reports both.
//
// Every other fixture here has one origin on the handling branch, so its surviving set and its
// fatal set are equal and the two readings agree. This is the only one that separates them.
int conjunctive_fatal_bad(mr::VulkanContextProvider* p, DesktopView& v) {
  Rhi* rhi = p->rhi();
  PanelAppHost* host = v.GetPanelAppHost();
  if (rhi == nullptr) {
    if (host == nullptr) {
      ::applog::logFatal("neither available");
    }
    fmtlike::log_error("rhi missing");
    return -1;
  }
  return rhi->x;
}

// Ordering control for the one above. The handled value's early return happens before the fatal
// value is even obtained, so no surviving path can pin rhi to null and no reading of the clause
// can suppress this. If this ever goes silent the pair above proves nothing.
int handled_before_bad(mr::VulkanContextProvider* p, DesktopView& v) {
  PanelAppHost* host = v.GetPanelAppHost();
  if (host == nullptr) {
    return -1;
  }
  Rhi* rhi = p->rhi();
  CHECK_NOTNULL(rhi);
  return rhi->x + host->id;
}

// Keeps "no fatal exit happens WITHOUT it" under its own test. Both fatal exits are the SAME
// call site, reached with the value null and with it non-null, and no path returns with it null
// -- so this is rejected by that clause alone, and a mutation that drops it shows up here rather
// than hiding behind the handled-branch clause the way unrelated_fatal_ok now does.
int shared_fatal_site_ok(mr::VulkanContextProvider* p, int mode) {
  Rhi* rhi = p->rhi();
  if (rhi == nullptr || mode < 0) {
    ::applog::logFatal("unusable");
  }
  return rhi->x;
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
