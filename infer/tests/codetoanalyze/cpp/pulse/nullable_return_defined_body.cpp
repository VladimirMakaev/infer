/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// A function on the nullable-return list may also be one Pulse can analyze. The list says the
// function MAY return null; a body has already answered that question for itself, so the entry is
// only worth acting on when some path of the body agrees.
//
// Two shapes have to be told apart, and the second is why "skip the model whenever the callee has
// a body" would be wrong:
//   - the error path is DEAD: no path returns null, so applying the model reports a path the body
//     rules out;
//   - the error path is LIVE but conditional: the body does return null, yet only under a
//     condition, so the summary's own null is latent and nothing is reported without the model.
//
// The listed functions are at GLOBAL scope and start with `dangerous` because
// --pulse-model-return-nullable is anchored at the start of the procedure name. A namespaced name
// never matches, and then every case here passes while exercising nothing.
//
// No allocation anywhere: a `new` that is not freed reports a leak and buries the point.

namespace nullable_return_defined_body {
struct Session {
  int level;
};
Session the_session;
bool succeeded();
void log_failure();
void sink_int(int);
} // namespace nullable_return_defined_body

// DEAD error path: the failure branch is unreachable because the check always succeeds.
nullable_return_defined_body::Session* dangerous_dead_path() {
  if (false) {
    nullable_return_defined_body::log_failure();
    return nullptr;
  }
  return &nullable_return_defined_body::the_session;
}

// LIVE error path, conditional on a parameter: the body returns null, but only sometimes, so the
// summary's own null case is latent and would not be reported on its own.
nullable_return_defined_body::Session* dangerous_live_path(bool ready) {
  if (!ready) {
    nullable_return_defined_body::log_failure();
    return nullptr;
  }
  return &nullable_return_defined_body::the_session;
}

// every path returns null
nullable_return_defined_body::Session* dangerous_always_null() { return nullptr; }

// declared, never defined: Pulse has no summary, so the list entry is all there is
nullable_return_defined_body::Session* dangerous_no_body();

namespace nullable_return_defined_body {

// the body rules the null out, so this must be silent
int dead_error_path_ok() { return dangerous_dead_path()->level; }

// the body really can return null, so this must keep reporting
int live_error_path_bad(bool ready) { return dangerous_live_path(ready)->level; }

int always_null_bad() { return dangerous_always_null()->level; }

// ... and a caller that checks is silent
int live_error_path_checked_ok(bool ready) {
  Session* s = dangerous_live_path(ready);
  if (s == nullptr) {
    return -1;
  }
  return s->level;
}

// the case the list exists for: no body at all
int no_body_bad() { return dangerous_no_body()->level; }

int no_body_checked_ok() {
  Session* s = dangerous_no_body();
  if (s == nullptr) {
    return -1;
  }
  return s->level;
}


// ---------------- the callee's other effects must survive ----------------
//
// When the body rules the null out we apply the callee's real summary rather than dropping the
// call. These pin that: an earlier version returned a no-op instead, which silently discarded
// everything else the callee does.

} // namespace nullable_return_defined_body

namespace nullable_return_defined_body {
int side_global = 0;
}

// returns non-null on every path, but dereferences null on the way
nullable_return_defined_body::Session* dangerous_internal_npe() {
  int* p = nullptr;
  nullable_return_defined_body::sink_int(*p);
  return &nullable_return_defined_body::the_session;
}

// returns non-null on every path, but dereferences its own parameter
nullable_return_defined_body::Session* dangerous_derefs_param(int* p) {
  nullable_return_defined_body::sink_int(*p);
  return &nullable_return_defined_body::the_session;
}

// returns non-null on every path and initialises an out-parameter
nullable_return_defined_body::Session* dangerous_sets_ptr(int** out) {
  *out = &nullable_return_defined_body::side_global;
  return &nullable_return_defined_body::the_session;
}

// returns non-null on every path and writes a global
nullable_return_defined_body::Session* dangerous_writes_global() {
  nullable_return_defined_body::side_global = 7;
  return &nullable_return_defined_body::the_session;
}

namespace nullable_return_defined_body {

// The callee's own dereference is a real bug and is reported IN THE CALLEE, once, which is what
// stock Pulse does for any defined callee. The caller is silent, and that is correct rather than
// a lost report: the only thing the caller dereferences is the RETURN value, and the body returns
// the address of a global on every path, so a caller-side report here could only come from the
// null the list asserts and the body disproves -- the very thing this change removes. (Verified
// from the qualifier text: the caller-side report reads "`dangerous_internal_npe()` could be
// null", not the callee's "`p` could be null".)
int internal_npe_caller_ok() { return dangerous_internal_npe()->level; }

// passing null to a callee that dereferences it is a real bug and must keep reporting
int derefs_param_caller_bad() {
  int* q = nullptr;
  dangerous_derefs_param(q);
  return 0;
}

// the out-parameter write must still happen, or the caller's p stays null and we invent a report
int outparam_caller_ok() {
  int* p = nullptr;
  dangerous_sets_ptr(&p);
  return *p;
}

// the global write must still happen
int writes_global_caller_ok() {
  nullable_return_defined_body::side_global = 0;
  dangerous_writes_global();
  return side_global;
}

} // namespace nullable_return_defined_body
