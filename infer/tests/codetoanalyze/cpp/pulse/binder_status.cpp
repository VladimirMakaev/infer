/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <string>

// The C++ status the AIDL C++ backend returns. Its accessors are inline, but
// every factory is defined in libbinder, so without models a status arrives
// with its fields unwritten and no check on it decides anything.
namespace android {

typedef int status_t;
enum {
  OK = 0,
  UNKNOWN_ERROR = (-2147483647 - 1),
  PERMISSION_DENIED = -1,
  FAILED_TRANSACTION = (UNKNOWN_ERROR + 2),
  UNEXPECTED_NULL = (UNKNOWN_ERROR + 8),
};

class String8 {
 public:
  String8() = default;
  String8(const char* s) : mStr(s == nullptr ? "" : s) {}
  const char* c_str() const { return mStr.c_str(); }

 private:
  std::string mStr;
};

namespace binder {
class Status final {
 public:
  enum Exception {
    EX_NONE = 0,
    EX_SECURITY = -1,
    EX_ILLEGAL_ARGUMENT = -3,
    EX_NULL_POINTER = -4,
    EX_ILLEGAL_STATE = -5,
    EX_SERVICE_SPECIFIC = -8,
    EX_TRANSACTION_FAILED = -129,
  };

  static Status ok();
  static Status fromExceptionCode(int exceptionCode);
  static Status fromExceptionCode(int exceptionCode, const String8& message);
  static Status fromExceptionCode(int exceptionCode, const char* message);
  static Status fromServiceSpecificError(int serviceSpecificErrorCode);
  static Status fromServiceSpecificError(int serviceSpecificErrorCode,
                                         const char* message);
  static Status fromStatusT(status_t status);

  Status() = default;
  ~Status() = default;
  Status(const Status& status) = default;
  Status(Status&& status) = default;
  Status& operator=(const Status& status) = default;

  int exceptionCode() const { return mException; }
  const String8& exceptionMessage() const { return mMessage; }
  status_t transactionError() const {
    return mException == EX_TRANSACTION_FAILED ? mErrorCode : OK;
  }
  int serviceSpecificErrorCode() const {
    return mException == EX_SERVICE_SPECIFIC ? mErrorCode : 0;
  }
  bool isOk() const { return mException == EX_NONE; }

 private:
  Status(int exceptionCode, int errorCode);
  int mException = EX_NONE;
  int mErrorCode = 0;
  String8 mMessage;
};
} // namespace binder
} // namespace android

namespace binder_status {

using android::binder::Status;

// Each test dereferences null only on the branch the models have to decide. An
// _ok is a branch that cannot be taken once the factories are modelled; a _bad
// is one that genuinely can.

void ok_is_ok_ok() {
  Status s = Status::ok();
  if (!s.isOk()) {
    int* p = nullptr;
    *p = 1;
  }
}

// TRUE-POSITIVE CONTROL: a security failure is not ok, so this branch is live
// and must keep reporting. Without it the silences could equally come from a
// model that made every status unreachable.
void security_is_not_ok_bad() {
  Status s = Status::fromExceptionCode(Status::EX_SECURITY);
  if (!s.isOk()) {
    int* p = nullptr;
    *p = 1;
  }
}

void security_code_ok() {
  Status s = Status::fromExceptionCode(Status::EX_SECURITY,
                                       "caller lacks a permission");
  if (s.exceptionCode() != Status::EX_SECURITY) {
    int* p = nullptr;
    *p = 1;
  }
}

// A SECOND CONTROL on the same accessor as the silence above: without it,
// "stored EX_SECURITY" and "stored anything at all" look the same.
void security_is_not_illegal_state_bad() {
  Status s = Status::fromExceptionCode(Status::EX_SECURITY);
  if (s.exceptionCode() != Status::EX_ILLEGAL_STATE) {
    int* p = nullptr;
    *p = 1;
  }
}

void service_specific_ok() {
  Status s = Status::fromServiceSpecificError(3);
  if (s.serviceSpecificErrorCode() != 3) {
    int* p = nullptr;
    *p = 1;
  }
}

// serviceSpecificErrorCode() is inline and returns 0 unless the exception is
// EX_SERVICE_SPECIFIC, so a plain exception status reads 0 through it.
void exception_has_no_service_specific_code_ok() {
  Status s = Status::fromExceptionCode(Status::EX_SECURITY);
  if (s.serviceSpecificErrorCode() != 0) {
    int* p = nullptr;
    *p = 1;
  }
}

void from_status_t_ok_is_ok_ok() {
  Status s = Status::fromStatusT(android::OK);
  if (!s.isOk()) {
    int* p = nullptr;
    *p = 1;
  }
}

void from_status_t_permission_ok() {
  Status s = Status::fromStatusT(android::PERMISSION_DENIED);
  if (s.transactionError() != android::PERMISSION_DENIED) {
    int* p = nullptr;
    *p = 1;
  }
}

// setFromStatusT has NO case for any particular status value: every non-OK status becomes a
// failed transaction that keeps the status as its error code. UNEXPECTED_NULL is the status
// most likely to be assumed special - an earlier version of this model invented an
// EX_NULL_POINTER arm for it, which silently deleted the two reports below - so it is pinned
// here from both sides.
void from_status_t_unexpected_null_ok() {
  Status s = Status::fromStatusT(android::UNEXPECTED_NULL);
  if (s.exceptionCode() != Status::EX_TRANSACTION_FAILED ||
      s.transactionError() != android::UNEXPECTED_NULL) {
    int* p = nullptr;
    *p = 1;
  }
}

// The invented arm, pinned as dead: a status built this way is never EX_NULL_POINTER.
void from_status_t_unexpected_null_is_not_null_pointer_ok() {
  Status s = Status::fromStatusT(android::UNEXPECTED_NULL);
  if (s.exceptionCode() == Status::EX_NULL_POINTER) {
    int* p = nullptr;
    *p = 1;
  }
}

// The two branches that invented arm used to delete. Both are live under libbinder, so both
// must keep reporting: these are the regression controls for the whole deviation.
void from_status_t_unexpected_null_is_transaction_failed_bad() {
  Status s = Status::fromStatusT(android::UNEXPECTED_NULL);
  if (s.exceptionCode() == Status::EX_TRANSACTION_FAILED) {
    int* p = nullptr;
    *p = 1;
  }
}

void from_status_t_unexpected_null_has_error_code_bad() {
  Status s = Status::fromStatusT(android::UNEXPECTED_NULL);
  if (s.transactionError() != android::OK) {
    int* p = nullptr;
    *p = 1;
  }
}

// libbinder's one special case in fromExceptionCode: EX_TRANSACTION_FAILED carries
// FAILED_TRANSACTION as its error code, so transactionError() hands that back rather than OK.
void from_exception_code_transaction_failed_ok() {
  Status s = Status::fromExceptionCode(Status::EX_TRANSACTION_FAILED);
  if (s.transactionError() != android::FAILED_TRANSACTION) {
    int* p = nullptr;
    *p = 1;
  }
}

void from_exception_code_transaction_failed_is_not_ok_bad() {
  Status s = Status::fromExceptionCode(Status::EX_TRANSACTION_FAILED);
  if (s.transactionError() != android::OK) {
    int* p = nullptr;
    *p = 1;
  }
}

// A transport failure is not ok: the other side of from_status_t_ok_is_ok_ok.
void from_status_t_permission_is_not_ok_bad() {
  Status s = Status::fromStatusT(android::PERMISSION_DENIED);
  if (!s.isOk()) {
    int* p = nullptr;
    *p = 1;
  }
}

// The copy constructor is defaulted and inline, so a copy keeps the fields once
// the factories are modelled. The copy is reassigned after it is read, which
// makes it a copy the program genuinely needs, so this case tests copy
// semantics on its own instead of also tripping the unnecessary-copy checker -
// that interaction has its own case below.
void copy_keeps_code_ok() {
  Status a = Status::fromExceptionCode(Status::EX_ILLEGAL_ARGUMENT);
  Status b = a;
  if (b.exceptionCode() != Status::EX_ILLEGAL_ARGUMENT) {
    int* p = nullptr;
    *p = 1;
  }
  b = Status::ok();
}

// MODELLING THE FACTORIES MAKES OTHER CHECKERS SEE THROUGH A STATUS TOO, and
// this case records that rather than leaving it to be discovered. Before the
// models the factory was an unknown call, so the status was opaque and the
// unnecessary-copy analysis could conclude nothing; now the copy below is
// visibly never modified and is reported. The report is correct - a const
// reference would do - but it is a report this file would not have had, so it
// is pinned here deliberately.
void unnecessary_copy_of_status_bad() {
  Status a = Status::fromExceptionCode(Status::EX_ILLEGAL_ARGUMENT);
  Status b = a;
  if (b.exceptionCode() != Status::EX_ILLEGAL_ARGUMENT) {
    int* p = nullptr;
    *p = 1;
  }
}

// A status this module did not build keeps unknown contents, so BOTH sides of a
// check on it stay feasible. These are the guard against the models
// over-claiming: if they fall silent, the models are deciding statuses they
// know nothing about, which hides real bugs rather than noise.
Status opaque_status();

void unknown_status_may_be_not_ok_bad() {
  Status s = opaque_status();
  if (!s.isOk()) {
    int* p = nullptr;
    *p = 1;
  }
}

void unknown_status_may_be_ok_bad() {
  Status s = opaque_status();
  if (s.isOk()) {
    int* p = nullptr;
    *p = 1;
  }
}

// A non-literal exception code is kept symbolically: compared against itself it
// is decided, against another value it is not.
void symbolic_exception_code_ok(int code) {
  Status s = Status::fromExceptionCode(code);
  if (s.exceptionCode() != code) {
    int* p = nullptr;
    *p = 1;
  }
}

void symbolic_exception_code_may_differ_bad(int code) {
  Status s = Status::fromExceptionCode(code);
  if (s.exceptionCode() != Status::EX_SECURITY) {
    int* p = nullptr;
    *p = 1;
  }
}

} // namespace binder_status
