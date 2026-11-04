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

} // namespace hardware
} // namespace android

namespace hidl_return_status {

using android::hardware::Return;
using android::hardware::Status;

// A return whose origin the analysis cannot see. Everything about it, including
// whether it was already checked, is unknown.
Return<int> opaque_return();

// ---------------------------------------------------------------------------
// What the models DO decide. Both of these build the return locally, so the
// checked flag is known false from the member initialiser and the status is
// known failed from the guard above it.

// The destructor aborts an unchecked failure, so nothing after the scope runs
// on the failing path.
void explicitly_failed_unchecked_dtor_aborts_ok(Status s) {
  if (s.isOk()) {
    return;
  }
  bool transport_ok;
  {
    Return<int> r(s);
    transport_ok = r.isOkUnchecked(); // does NOT mark the status checked
  } // ~return_status aborts here: unchecked and the transport failed
  if (!transport_ok) {
    int* p = nullptr;
    *p = 1; // unreachable: the destructor above aborted
  }
}

// Converting a return to its value aborts a failed transport the same way.
void explicitly_failed_value_retrieval_ok(Status s) {
  if (s.isOk()) {
    return;
  }
  Return<int> r(s);
  int v = r; // onValueRetrieval() aborts: the transport failed
  int* p = nullptr;
  *p = v; // unreachable: the conversion above aborted
}

// ---------------------------------------------------------------------------
// TRUE-POSITIVE CONTROLS. Each pairs with a silence above and differs in one
// thing only, so a model that simply pruned everything would delete these.

// Same shape as explicitly_failed_unchecked_dtor_aborts_ok, except the status is
// CHECKED. A checked failure does not abort, so this branch is live.
void explicitly_failed_then_checked_bad(Status s) {
  if (s.isOk()) {
    return;
  }
  bool transport_ok;
  {
    Return<int> r(s);
    transport_ok = r.isOk(); // marks the status checked
  } // no abort: the caller looked at it
  if (!transport_ok) {
    int* p = nullptr;
    *p = 1; // reachable
  }
}

// Same shape as explicitly_failed_value_retrieval_ok, except the transport
// SUCCEEDED, so reading the value does not abort and the code after it runs.
void ok_value_retrieval_still_reports_bad(Status s) {
  if (!s.isOk()) {
    return;
  }
  Return<int> r(s);
  int v = r; // no abort: the transport succeeded
  int* p = nullptr;
  *p = v; // reachable
}

// The oracle's own control, on a return whose origin is unknown: isOk() marks it
// checked, so the destructor must not abort and this must keep reporting.
void checked_return_dtor_does_not_abort_bad() {
  bool transport_ok;
  {
    Return<int> r = opaque_return();
    transport_ok = r.isOk();
  }
  if (!transport_ok) {
    int* p = nullptr;
    *p = 1; // reachable
  }
}

// The oracle's move control: the moved-to object is checked, so its destructor
// does not abort.
void moved_then_checked_bad() {
  bool transport_ok;
  {
    Return<int> a = opaque_return();
    Return<int> b = std::move(a);
    transport_ok = b.isOk();
  }
  if (!transport_ok) {
    int* p = nullptr;
    *p = 1; // reachable
  }
}

// OVER-CLAIMING GUARD: a return from an opaque source may equally be ok, so both
// sides of a check on it must stay feasible. If this ever falls silent the
// models are deciding returns they know nothing about.
void unknown_origin_may_be_ok_bad() {
  Return<int> r = opaque_return();
  bool transport_ok = r.isOk();
  if (transport_ok) {
    int* p = nullptr;
    *p = 1; // reachable
  }
}

// ---------------------------------------------------------------------------
// KNOWN LIMITATIONS. All three have the same single cause: the return is born
// from a call the analysis cannot see, so its checked flag is unconstrained and
// the destructor cannot prove the abort happens. Deciding an unproven flag is
// false would remove all three, but it also deletes the true positives above -
// measured at the time: every control fell silent - so it is not done here.

// FP_: unchecked failure from an opaque source. The real destructor aborts.
void FP_unknown_origin_unchecked_failure_ok() {
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

// FP_: isDeadObject() marks the status checked only when the error really is
// DEAD_OBJECT, so on the not-dead path the return is still unchecked.
void FP_dead_object_partial_check_ok() {
  bool dead;
  bool transport_ok;
  {
    Return<int> r = opaque_return();
    dead = r.isDeadObject();
    transport_ok = r.isOkUnchecked();
  }
  if (!transport_ok && !dead) {
    int* p = nullptr;
    *p = 1; // FP: unchecked and failed, so the real destructor aborted
  }
}

// FP_: the move assignment is not modelled, so the moved-to return keeps an
// unknown status and an unknown checked flag.
void FP_moved_unchecked_failure_ok() {
  bool transport_ok;
  {
    Return<int> a = opaque_return();
    Return<int> b = std::move(a);
    transport_ok = b.isOkUnchecked();
  }
  if (!transport_ok) {
    int* p = nullptr;
    *p = 1; // FP: the real destructor of b aborted
  }
}

} // namespace hidl_return_status
