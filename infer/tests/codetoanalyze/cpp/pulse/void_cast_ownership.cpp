/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// A discarded-value expression `(void) e` is translated to a call to a frontend builtin so that
// the READ of `e` stays visible to liveness. That builtin used to be [__infer_skip], which Pulse
// models as an unknown call; for a pointer operand an unknown call takes ownership, so the
// allocation attribute was dropped and the leak was never reported -- permanently, even if the
// pointer was dereferenced afterwards. `(void) p` is the standard idiom for silencing
// unused-variable warnings and is what UNUSED()/IGNORE() macros expand to.
//
// This file is also compiled with -std=c++11 by the pulse-11 case, so keep it C++11-clean.

#include <cstdio>
#include <memory>

namespace void_cast_ownership {

void sink(int* p);

// ----------------------------- the bug: these must report -----------------------------

void void_cast_leak_bad() {
  int* p = new int(1);
  (void)p;
}

void static_cast_to_void_leak_bad() {
  int* p = new int(1);
  static_cast<void>(p);
}

// the suppression used to survive any later use: the allocation was already gone
void void_cast_then_deref_leak_bad() {
  int* p = new int(1);
  (void)p;
  *p = 3;
}

// through an alias rather than the original pointer
void void_cast_alias_leak_bad() {
  int* p = new int(1);
  int* q = p;
  (void)q;
}

// the shape an UNUSED()-style macro expands to
#define VOID_CAST_UNUSED(x) \
  do {                      \
    (void)(x);              \
  } while (0)

void void_cast_in_macro_leak_bad() {
  int* p = new int(1);
  VOID_CAST_UNUSED(p);
}

void void_cast_array_leak_bad() {
  int* p = new int[4];
  (void)p;
}

// not only memory: a FILE* handed to a void cast was dropped the same way
void void_cast_file_leak_bad() {
  FILE* f = fopen("/tmp/void_cast_ownership", "r");
  (void)f;
}

void void_cast_released_unique_ptr_leak_bad() {
  std::unique_ptr<int> u(new int(1));
  int* r = u.release();
  (void)r;
}

// ----------------------------- controls that must NOT move -----------------------------

// THE control for this fix: an unknown callee really may take ownership, so this stays silent.
// If this starts reporting, the fix has over-corrected and made genuine escapes into leaks.
void escape_to_unknown_callee_ok() {
  int* p = new int(1);
  sink(p);
}

// a void cast of something that owns nothing
void void_cast_non_owning_ok() {
  int x = 0;
  (void)x;
}

// the allocation is freed; the void cast must not invent a leak
void void_cast_then_delete_ok() {
  int* p = new int(1);
  (void)p;
  delete p;
}

// unique_ptr still owns it, so there is no leak
void void_cast_owned_by_unique_ptr_ok() {
  std::unique_ptr<int> u(new int(1));
  (void)u;
}

// plain leak with no void cast anywhere: the baseline that was always reported
void plain_leak_bad() { int* p = new int(1); }

// ---- site-2 control: pointer-to-member access/assign also emits a frontend skip builtin, but
// ---- a different one ([__infer_skip]), because there the unknown callee legitimately absorbs
// ---- the stored resource. These must stay silent.
struct Holder {
  int* res;
};

void ptr_to_member_assign_ok(Holder& h, int* Holder::*pm) { h.*pm = new int(1); }

void ptr_to_member_read_ok(Holder& h, int* Holder::*pm) {
  int* q = h.*pm;
  (void)q;
}

} // namespace void_cast_ownership
