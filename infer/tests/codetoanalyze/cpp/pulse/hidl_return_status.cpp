/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <utility>

// The HIDL Return<T>. A caller is required to look at the transport status: the
// destructor of an unchecked return whose transport failed calls LOG(FATAL), and
// converting a return to its value aborts the same way. Those three functions
// live in a shared library, so only their declarations are visible here - which
// is exactly the situation the models are for, and why they are DECLARED and not
// defined below.
namespace android {

typedef int status_t;

namespace hardware {

enum {
  STATUS_OK = 0,
  DEAD_OBJECT = -32,
};

class Status final {
 public:
  enum Exception {
    EX_NONE = 0,
    EX_SECURITY = -1,
    EX_TRANSACTION_FAILED = -129,
  };

  Status() = default;
  ~Status() = default;
  Status(const Status& status) = default;
  Status& operator=(const Status& status) = default;

  int exceptionCode() const { return mException; }
  status_t transactionError() const {
    return mException == EX_TRANSACTION_FAILED ? mErrorCode : STATUS_OK;
  }
  bool isOk() const { return mException == EX_NONE; }

 private:
  int mException = EX_NONE;
  int mErrorCode = 0;
};

namespace details {
class return_status {
 private:
  Status mStatus{};
  mutable bool mCheckedStatus = false;

 protected:
  // defined in the shared library: aborts when the transport failed
  void onValueRetrieval() const;

 public:
  // defined in the shared library: aborts when the transport failed
  void assertOk() const;
  return_status() {}
  return_status(const Status& s) : mStatus(s) {}
  return_status(const return_status&) = delete;
  return_status& operator=(const return_status&) = delete;
  return_status(return_status&& other) noexcept { *this = std::move(other); }
  // defined in the shared library
  return_status& operator=(return_status&& other) noexcept;
  // defined in the shared library: aborts when unchecked AND the transport failed
  ~return_status();

  // these three ARE inline, so nothing on the reading side is modelled
  bool isOkUnchecked() const { return mStatus.isOk(); }
  bool isOk() const {
    mCheckedStatus = true;
    return mStatus.isOk();
  }
  bool isDeadObject() const {
    bool dead = mStatus.transactionError() == DEAD_OBJECT;
    if (dead) {
      mCheckedStatus = true;
    }
    return dead;
  }
};
} // namespace details

template <typename T>
class Return : public details::return_status {
 private:
  T mVal{};

 public:
  Return(T v) : details::return_status(), mVal{v} {}
  Return(Status s) : details::return_status(s) {}
  Return(Return&& other) noexcept = default;
  Return& operator=(Return&&) noexcept = default;
  ~Return() = default;
  operator T() const {
    onValueRetrieval();
    return mVal;
  }
};

// A generated HIDL interface is android::hardware::<package>::V<major>_<minor>::I<Name> and its
// transport methods are pure virtual. Those are the producers: a transport call hands back a
// return it has just built, so its checked flag is false. An interface also carries methods that
// are NOT transport calls, and the three shapes below are each kept off the model by a different
// part of it.
Status opaqueStatus();

namespace testhal {
namespace V1_0 {
struct ITestHal {
  // A STATIC WITH A BODY, the shape of a generated castFrom. Its return type is Return and its
  // class name starts with I, so a matcher keyed on those alone would displace this body.
  static Return<int> castFrom(int tag) {
    Return<int> r(opaqueStatus());
    r.isOk();
    return r;
  }

  // A STATIC WITH NO BODY, the shape of a generated getService whose definition is in the shared
  // library. Nothing is there to displace, so definedness says nothing about it; only requiring
  // the receiver to BE the interface keeps it unmodelled.
  static Return<int> remoteCastFrom(int tag);

  // A NON-STATIC method WITH A BODY. The receiver is the interface, so the matcher does match it,
  // and only definedness keeps the body.
  Return<int> defaultedHelper(int tag) {
    Return<int> r(opaqueStatus());
    r.isOk();
    return r;
  }

  // The same, with a body that fixes the value as well as the status. Analysing it proves
  // v == 5; replacing the call - even with nothing more than an unknown effect - does not.
  Return<int> okHelper() {
    Return<int> r(5);
    r.isOk();
    return r;
  }

  virtual Return<int> getValue() = 0;
  virtual Return<int> configure(int setting, bool enable) = 0;
};

// The concrete proxy generated beside the interface. It is deliberately NOT matched: it has a
// body in a capture, and a model replacing it would discard everything the body does - including,
// here, the fact that the proxy checked the status itself.
struct BpHwTestHal : public ITestHal {
  Return<int> getValue() override;
  Return<int> configure(int setting, bool enable) override;
  Status unknownStatus();
  Return<int> alreadyChecked() {
    Return<int> r(unknownStatus());
    r.isOk();
    return r;
  }
};
} // namespace V1_0
} // namespace testhal

// A two-component package name, the other shape the matchers have to cover.
namespace graphics {
namespace testcomp {
namespace V2_1 {
struct ITestComp {
  virtual Return<int> present() = 0;
};
} // namespace V2_1
} // namespace testcomp
} // namespace graphics

} // namespace hardware
} // namespace android

namespace hidl_return_status {

using android::hardware::Return;
using android::hardware::Status;

// A return whose origin the analysis cannot see and which is NOT a HIDL interface call.
// Everything about it, including whether it was already checked, stays unknown.
Return<int> opaque_return();

using android::hardware::graphics::testcomp::V2_1::ITestComp;
using android::hardware::testhal::V1_0::BpHwTestHal;
using android::hardware::testhal::V1_0::ITestHal;

// ---------------------------------------------------------------------------
// A HIDL interface call supplies the one fact the destructor was missing: the return it hands
// back has just been built, so its checked flag is false. The transport status stays unknown -
// that is what the caller still has to decide.

void hidl_unchecked_failure_ok(ITestHal* hal) {
  bool transport_ok;
  {
    Return<int> r = hal->getValue();
    transport_ok = r.isOkUnchecked(); // does NOT mark the status checked
  } // ~return_status aborts here: unchecked and the transport failed
  if (!transport_ok) {
    int* p = nullptr;
    *p = 1; // unreachable
  }
}

// isDeadObject() marks the status checked ONLY when the error really is DEAD_OBJECT, so the
// not-dead path is still unchecked and still aborts.
void hidl_dead_object_partial_check_ok(ITestHal* hal) {
  bool dead;
  bool transport_ok;
  {
    Return<int> r = hal->getValue();
    dead = r.isDeadObject();
    transport_ok = r.isOkUnchecked();
  }
  if (!transport_ok && !dead) {
    int* p = nullptr;
    *p = 1; // unreachable
  }
}

// The move assignment validates the destination and then SWAPS, so the moved-to object carries
// the unchecked failure and aborts, while the moved-from one is left safe to destroy.
void hidl_moved_unchecked_failure_ok(ITestHal* hal) {
  bool transport_ok;
  {
    Return<int> a = hal->configure(1, true);
    Return<int> b = std::move(a);
    transport_ok = b.isOkUnchecked();
  }
  if (!transport_ok) {
    int* p = nullptr;
    *p = 1; // unreachable
  }
}

// The other package shape: android::hardware::<a>::<b>::V<n>_<m>::I<Name>.
void hidl_two_component_package_ok(ITestComp* comp) {
  bool transport_ok;
  {
    Return<int> r = comp->present();
    transport_ok = r.isOkUnchecked();
  }
  if (!transport_ok) {
    int* p = nullptr;
    *p = 1; // unreachable
  }
}

// ---------------------------------------------------------------------------
// The same two shapes with the return built locally rather than by a transport call, so they
// hold even where no producer is involved.

void explicitly_failed_unchecked_dtor_aborts_ok(Status s) {
  if (s.isOk()) {
    return;
  }
  bool transport_ok;
  {
    Return<int> r(s);
    transport_ok = r.isOkUnchecked();
  }
  if (!transport_ok) {
    int* p = nullptr;
    *p = 1; // unreachable
  }
}

void explicitly_failed_value_retrieval_ok(Status s) {
  if (s.isOk()) {
    return;
  }
  Return<int> r(s);
  int v = r; // onValueRetrieval() aborts: the transport failed
  int* p = nullptr;
  *p = v; // unreachable
}

// ---------------------------------------------------------------------------
// TRUE-POSITIVE CONTROLS. Each differs from a silence above in exactly one thing, so a model
// that simply pruned everything would delete these instead of looking like an improvement.

// The oracle's control: isOk() marks the return checked, so the destructor must NOT abort.
void hidl_checked_return_dtor_does_not_abort_bad(ITestHal* hal) {
  bool transport_ok;
  {
    Return<int> r = hal->getValue();
    transport_ok = r.isOk(); // marks it checked
  }
  if (!transport_ok) {
    int* p = nullptr;
    *p = 1; // reachable
  }
}

// The oracle's move control: the moved-to object is checked, so destroying it does not abort.
void hidl_moved_then_checked_bad(ITestHal* hal) {
  bool transport_ok;
  {
    Return<int> a = hal->configure(2, false);
    Return<int> b = std::move(a);
    transport_ok = b.isOk();
  }
  if (!transport_ok) {
    int* p = nullptr;
    *p = 1; // reachable
  }
}

// OVER-CLAIMING GUARD: modelling the producer fixes the CHECKED flag, not the status. A HIDL
// call may equally have succeeded, so both sides of a check must stay feasible.
void hidl_status_may_be_ok_bad(ITestHal* hal) {
  Return<int> r = hal->getValue();
  bool transport_ok = r.isOk();
  if (transport_ok) {
    int* p = nullptr;
    *p = 1; // reachable
  }
}

void explicitly_failed_then_checked_bad(Status s) {
  if (s.isOk()) {
    return;
  }
  bool transport_ok;
  {
    Return<int> r(s);
    transport_ok = r.isOk(); // marks the status checked
  }
  if (!transport_ok) {
    int* p = nullptr;
    *p = 1; // reachable
  }
}

void ok_value_retrieval_still_reports_bad(Status s) {
  if (!s.isOk()) {
    return;
  }
  Return<int> r(s);
  int v = r; // no abort: the transport succeeded
  int* p = nullptr;
  *p = v; // reachable
}

// CONTROL THAT THE CONCRETE PROXY IS NOT MODELLED. BpHwTestHal::alreadyChecked has a body that
// checks the status itself, so the return it yields is CHECKED and its destructor does not
// abort - this must report. If the models ever matched concrete proxies, that body would be
// replaced by "freshly constructed, unchecked", the destructor would abort, and this would fall
// silent. That silence is the regression this case exists to catch.
void concrete_proxy_is_not_modelled_bad(BpHwTestHal* proxy) {
  bool transport_ok;
  {
    Return<int> r = proxy->alreadyChecked();
    transport_ok = r.isOkUnchecked();
  }
  if (!transport_ok) {
    int* p = nullptr;
    *p = 1; // reachable: the proxy already checked the status
  }
}

// ---------------------------------------------------------------------------
// A METHOD THAT HAS A BODY KEEPS IT. A model replaces the call it matches, so claiming a freshly
// built return for a method the program can see through would delete whatever its body
// established. Each case below is a true positive that exactly one part of the guard preserves,
// and each of them was silent before that part existed.

// castFrom is a static on the I-class whose body checks its own return, so the return it yields
// is already checked, its destructor does not abort, and this branch is live. Displacing the body
// makes the return read freshly-unchecked, the destructor provably aborting, and this SILENT.
void castfrom_body_already_checked_bad() {
  bool transport_ok;
  {
    Return<int> r = ITestHal::castFrom(7);
    transport_ok = r.isOkUnchecked();
  }
  if (!transport_ok) {
    int* p = nullptr;
    *p = 1; // reachable: castFrom's body already checked the status
  }
}

// The same static seen from the other side: its body checks the status, so a caller that checks
// again learns nothing new and both branches stay feasible.
void castfrom_status_may_be_ok_bad() {
  Return<int> r = ITestHal::castFrom(9);
  if (r.isOk()) {
    int* p = nullptr;
    *p = 1; // reachable
  }
}

// defaultedHelper is NOT static, so it is dispatched on the interface and the receiver type does
// not keep it off the model - its body does. A silence here means definedness is not consulted.
void defaulted_helper_body_already_checked_bad(ITestHal* hal) {
  bool transport_ok;
  {
    Return<int> r = hal->defaultedHelper(3);
    transport_ok = r.isOkUnchecked();
  }
  if (!transport_ok) {
    int* p = nullptr;
    *p = 1; // reachable: the helper's body already checked the status
  }
}

// THE PAIR THAT PINS THAT THE BODY IS REALLY ANALYSED, not merely left unknown. okHelper returns
// the value 5, so v != 5 is infeasible and this is silent; a call replaced by an unknown effect
// would lose the value and report. The _bad twin keeps the pair honest.
void okhelper_value_is_five_ok(ITestHal* hal) {
  int v = hal->okHelper();
  if (v != 5) {
    int* p = nullptr;
    *p = 1; // unreachable: the body returns 5
  }
}

void okhelper_value_is_not_five_bad(ITestHal* hal) {
  int v = hal->okHelper();
  if (v == 5) {
    int* p = nullptr;
    *p = 1; // reachable
  }
}

// ---------------------------------------------------------------------------
// THE REMAINING LIMITATIONS, kept honest in the suite.

// remoteCastFrom is a static with NO body, so there is nothing for definedness to protect; what
// keeps it unmodelled is that a transport call is dispatched on a receiver and a static has
// none. Its checked flag therefore stays unknown and the destructor cannot prove the abort. If
// this ever falls silent, a static is being treated as a transport call.
void FP_undefined_static_unchecked_failure_ok() {
  bool transport_ok;
  {
    Return<int> r = ITestHal::remoteCastFrom(5);
    transport_ok = r.isOkUnchecked();
  }
  if (!transport_ok) {
    int* p = nullptr;
    *p = 1; // FP: the real destructor aborted before reaching here
  }
}

// A producer that is not a HIDL interface call tells us nothing about the checked flag either.
// Widening the match to cover this would mean guessing at functions whose bodies we may have.
void FP_non_hidl_producer_unchecked_failure_ok() {
  bool transport_ok;
  {
    Return<int> r = opaque_return();
    transport_ok = r.isOkUnchecked();
  }
  if (!transport_ok) {
    int* p = nullptr;
    *p = 1; // FP: the real destructor aborted before reaching here
  }
}

} // namespace hidl_return_status
