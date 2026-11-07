// A nullable-return entry on a callee that returns std::shared_ptr or std::unique_ptr by value.
// Such a callee returns through a hidden parameter, so the value the caller tests is the MANAGED
// pointer inside the returned object rather than the return value itself. Until the origin was
// recorded on that managed pointer, none of these was a source and only the raw-pointer control
// below reported, whatever the fatal shape.

#include <memory>
#include <utility>

struct Rhi {
  int x;
};

namespace mr {
struct VulkanContextProvider {
  virtual std::shared_ptr<Rhi> rhi();     // listed
  virtual std::unique_ptr<Rhi> makeRhi(); // listed
  virtual Rhi* rawRhi();                  // listed, and the control that always worked
};
} // namespace mr

namespace applog {
[[noreturn]] void logFatal(const char* msg) noexcept;
}

#define XR_CHECK_NOTNULL(v) \
  ((v) != nullptr ? (void)0 : ::applog::logFatal(#v " is null"))
#define XR_PRECONDITION_NOTNULL(v)              \
  ([](auto&& g) -> decltype(g) {                \
    XR_CHECK_NOTNULL(g);                        \
    return std::forward<decltype(g)>(g);        \
  }(v))

// the origin has to survive .get()
int shared_ptr_get_bad(mr::VulkanContextProvider* p) {
  std::shared_ptr<Rhi> rhi = p->rhi();
  XR_PRECONDITION_NOTNULL(rhi.get());
  return rhi->x;
}

// ... and the smart pointer passed straight to the check
int shared_ptr_direct_bad(mr::VulkanContextProvider* p) {
  return XR_PRECONDITION_NOTNULL(p->rhi())->x;
}

// ... and operator bool
int shared_ptr_bool_bad(mr::VulkanContextProvider* p) {
  std::shared_ptr<Rhi> rhi = p->rhi();
  if (!rhi) {
    applog::logFatal("no rhi");
  }
  return rhi->x;
}

// unique_ptr takes the same path
int unique_ptr_bool_bad(mr::VulkanContextProvider* p) {
  std::unique_ptr<Rhi> rhi = p->makeRhi();
  if (!rhi) {
    applog::logFatal("no rhi");
  }
  return rhi->x;
}

// TP control: the raw-pointer source, which reported before this change and must keep doing so.
// If this one ever goes quiet the run proved nothing about the smart-pointer shapes.
int raw_pointer_control_bad(mr::VulkanContextProvider* p) {
  Rhi* rhi = p->rawRhi();
  if (!rhi) {
    applog::logFatal("no rhi");
  }
  return rhi->x;
}

// the null is handled instead of being fatal, so there is no fatal-on-nullable-return here
int shared_ptr_graceful_ok(mr::VulkanContextProvider* p) {
  std::shared_ptr<Rhi> rhi = p->rhi();
  if (!rhi) {
    return -1;
  }
  return rhi->x;
}

// a fatal reached for a reason unrelated to the nullable value
int shared_ptr_unrelated_fatal_ok(mr::VulkanContextProvider* p, int mode) {
  std::shared_ptr<Rhi> rhi = p->rhi();
  if (!rhi) {
    return -1;
  }
  if (mode < 0) {
    applog::logFatal("bad mode");
  }
  return rhi->x + mode;
}

// an unlisted callee: its contract is unknown, so a check on it says nothing
namespace mr {
struct OtherProvider {
  std::shared_ptr<Rhi> rhi();
};
} // namespace mr

int unlisted_shared_ptr_ok(mr::OtherProvider* p) {
  std::shared_ptr<Rhi> rhi = p->rhi();
  if (!rhi) {
    applog::logFatal("no rhi");
  }
  return rhi->x;
}
