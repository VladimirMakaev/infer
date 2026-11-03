/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <utility>

// The status object the NDK binder API hands out. The AStatus_* functions live
// in a shared library, so only these declarations are ever visible and every
// status a factory returns would otherwise be an unconstrained pointer, leaving
// both sides of a status check feasible.
extern "C" {
struct AStatus;
typedef struct AStatus AStatus;

enum {
  STATUS_OK = 0,
  STATUS_DEAD_OBJECT = -32,
};
typedef int binder_status_t;

enum {
  EX_NONE = 0,
  EX_SECURITY = -1,
  EX_ILLEGAL_STATE = -5,
  EX_SERVICE_SPECIFIC = -8,
  EX_TRANSACTION_FAILED = -129,
};
typedef int binder_exception_t;

AStatus* AStatus_newOk();
AStatus* AStatus_fromExceptionCode(binder_exception_t exception);
AStatus* AStatus_fromExceptionCodeWithMessage(binder_exception_t exception,
                                              const char* message);
AStatus* AStatus_fromServiceSpecificError(int serviceSpecific);
AStatus* AStatus_fromStatus(binder_status_t status);
bool AStatus_isOk(const AStatus* status);
binder_exception_t AStatus_getExceptionCode(const AStatus* status);
int AStatus_getServiceSpecificError(const AStatus* status);
binder_status_t AStatus_getStatus(const AStatus* status);
void AStatus_delete(AStatus* status);
}

// The owning wrapper callers actually use. Its bodies are inline in the real
// header too, so modelling the C functions is enough to make it precise.
namespace ndk {
class ScopedAStatus {
 public:
  explicit ScopedAStatus(AStatus* a = nullptr) : mT(a) {}
  ~ScopedAStatus() {
    if (mT != nullptr) AStatus_delete(mT);
  }
  ScopedAStatus(ScopedAStatus&& other) noexcept : mT(other.mT) {
    other.mT = nullptr;
  }
  ScopedAStatus(const ScopedAStatus&) = delete;

  AStatus* get() const { return mT; }
  bool isOk() const { return get() != nullptr && AStatus_isOk(get()); }
  binder_exception_t getExceptionCode() const {
    return AStatus_getExceptionCode(get());
  }
  int getServiceSpecificError() const {
    return AStatus_getServiceSpecificError(get());
  }
  binder_status_t getStatus() const { return AStatus_getStatus(get()); }

  static ScopedAStatus ok() { return ScopedAStatus(AStatus_newOk()); }
  static ScopedAStatus fromExceptionCode(binder_exception_t exception) {
    return ScopedAStatus(AStatus_fromExceptionCode(exception));
  }
  static ScopedAStatus fromServiceSpecificError(int serviceSpecific) {
    return ScopedAStatus(AStatus_fromServiceSpecificError(serviceSpecific));
  }
  static ScopedAStatus fromStatus(binder_status_t status) {
    return ScopedAStatus(AStatus_fromStatus(status));
  }

 private:
  AStatus* mT;
};
} // namespace ndk

namespace binder_ndk_status {

// Each test dereferences null only on the branch the models have to decide. An
// _Ok is a branch that cannot be taken once the status is modelled; a _Bad is
// one that genuinely can.

void ok_is_ok_ok() {
  ndk::ScopedAStatus s = ndk::ScopedAStatus::ok();
  if (!s.isOk()) {
    int* p = nullptr;
    *p = 1;
  }
}

// TRUE-POSITIVE CONTROL: a security failure really is not ok, so this branch is
// live and must keep reporting. Without it the silences above could equally be
// produced by a model that made every status unreachable.
void security_is_not_ok_bad() {
  ndk::ScopedAStatus s = ndk::ScopedAStatus::fromExceptionCode(EX_SECURITY);
  if (!s.isOk()) {
    int* p = nullptr;
    *p = 1;
  }
}

void security_exception_code_ok() {
  ndk::ScopedAStatus s = ndk::ScopedAStatus::fromExceptionCode(EX_SECURITY);
  if (s.getExceptionCode() != EX_SECURITY) {
    int* p = nullptr;
    *p = 1;
  }
}

// A SECOND TRUE-POSITIVE CONTROL, on the same accessor as the silence above: if
// the model stored the wrong code, or stored nothing, this would stop
// reporting. A silence-only fixture cannot tell "stored EX_SECURITY" from
// "stored anything at all".
void security_is_not_illegal_state_bad() {
  ndk::ScopedAStatus s = ndk::ScopedAStatus::fromExceptionCode(EX_SECURITY);
  if (s.getExceptionCode() != EX_ILLEGAL_STATE) {
    int* p = nullptr;
    *p = 1;
  }
}

void service_specific_code_ok() {
  ndk::ScopedAStatus s = ndk::ScopedAStatus::fromServiceSpecificError(7);
  if (s.getExceptionCode() != EX_SERVICE_SPECIFIC ||
      s.getServiceSpecificError() != 7) {
    int* p = nullptr;
    *p = 1;
  }
}

// The service-specific code is its own field: a status built from an exception
// code leaves it zero rather than copying the exception into it.
void exception_leaves_service_specific_zero_ok() {
  ndk::ScopedAStatus s = ndk::ScopedAStatus::fromExceptionCode(EX_SECURITY);
  if (s.getServiceSpecificError() != 0) {
    int* p = nullptr;
    *p = 1;
  }
}

void from_status_ok_is_ok_ok() {
  ndk::ScopedAStatus s = ndk::ScopedAStatus::fromStatus(STATUS_OK);
  if (!s.isOk()) {
    int* p = nullptr;
    *p = 1;
  }
}

void from_status_dead_object_ok() {
  ndk::ScopedAStatus s = ndk::ScopedAStatus::fromStatus(STATUS_DEAD_OBJECT);
  if (s.getExceptionCode() != EX_TRANSACTION_FAILED ||
      s.getStatus() != STATUS_DEAD_OBJECT) {
    int* p = nullptr;
    *p = 1;
  }
}

// A failed transport is not ok: the other side of from_status_ok_is_ok_ok.
void from_status_dead_object_is_not_ok_bad() {
  ndk::ScopedAStatus s = ndk::ScopedAStatus::fromStatus(STATUS_DEAD_OBJECT);
  if (!s.isOk()) {
    int* p = nullptr;
    *p = 1;
  }
}

// The move constructor is inline, so a moved status keeps its code once the
// factories are modelled.
void moved_status_keeps_code_ok() {
  ndk::ScopedAStatus a =
      ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
  ndk::ScopedAStatus b = std::move(a);
  if (b.getExceptionCode() != EX_ILLEGAL_STATE) {
    int* p = nullptr;
    *p = 1;
  }
}

// A status this module did not build keeps unknown contents, so BOTH sides of a
// check on it stay feasible. These two are the guard against the models
// over-claiming: if they ever fall silent, the models are deciding statuses
// they know nothing about, which would hide real bugs rather than noise.
AStatus* opaque_status();

void unknown_status_may_be_not_ok_bad() {
  ndk::ScopedAStatus s(opaque_status());
  if (!s.isOk()) {
    int* p = nullptr;
    *p = 1;
  }
}

void unknown_status_may_be_ok_bad() {
  ndk::ScopedAStatus s(opaque_status());
  if (s.isOk()) {
    int* p = nullptr;
    *p = 1;
  }
}

// A status whose exception code is not a literal keeps it symbolically: the
// comparison against the same value is decided, the one against another is not.
void symbolic_exception_code_ok(binder_exception_t code) {
  ndk::ScopedAStatus s = ndk::ScopedAStatus::fromExceptionCode(code);
  if (s.getExceptionCode() != code) {
    int* p = nullptr;
    *p = 1;
  }
}

void symbolic_exception_code_may_differ_bad(binder_exception_t code) {
  ndk::ScopedAStatus s = ndk::ScopedAStatus::fromExceptionCode(code);
  if (s.getExceptionCode() != EX_SECURITY) {
    int* p = nullptr;
    *p = 1;
  }
}

} // namespace binder_ndk_status
