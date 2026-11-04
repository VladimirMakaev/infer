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
int* side_global_ptr = nullptr;
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

// ---------------- the disjunct cap must not overrule the list ----------------
//
// Pulse under-approximates once a procedure reaches --pulse-max-disjuncts (20), so a big callee's
// summary can be missing paths. Reading "no surviving disjunct returns null" as "the body cannot
// return null" would let the cap silently cancel a list entry, so a summary at the cap is treated
// like a callee with no summary and the model is kept.

nullable_return_defined_body::Session* dangerous_capped(int a, int b, int c, int d, int e) {
  int n = 0;
  if (a) { n++; }
  if (b) { n++; }
  if (c) { n++; }
  if (d) { n++; }
  if (e) { n++; } // 2^5 paths, above the cap
  if (n == 0) {
    nullable_return_defined_body::log_failure();
    return nullptr;
  }
  return &nullable_return_defined_body::the_session;
}

// A capped callee still has side effects, and they have to survive. Replacing the call with the
// unknown-call model keeps the list entry but throws the summary away, so the global pointer and
// the out-parameter below stay null in the caller and each invents a dereference. Applying the
// summary and adding a null RETURN value on top keeps both.

nullable_return_defined_body::Session* dangerous_capped_global(int a, int b, int c, int d, int e) {
  int n = 0;
  if (a) {
    n++;
  }
  if (b) {
    n++;
  }
  if (c) {
    n++;
  }
  if (d) {
    n++;
  }
  if (e) {
    n++;
  } // 2^5 paths, above the cap
  nullable_return_defined_body::sink_int(n);
  nullable_return_defined_body::side_global_ptr = &nullable_return_defined_body::side_global;
  if (n == 0) {
    // the write above still happened on this path: that is the point of the control
    nullable_return_defined_body::log_failure();
    return nullptr;
  }
  return &nullable_return_defined_body::the_session;
}

nullable_return_defined_body::Session*
dangerous_capped_outparam(int a, int b, int c, int d, int e, int** out) {
  int n = 0;
  if (a) {
    n++;
  }
  if (b) {
    n++;
  }
  if (c) {
    n++;
  }
  if (d) {
    n++;
  }
  if (e) {
    n++;
  } // 2^5 paths, above the cap
  nullable_return_defined_body::sink_int(n);
  *out = &nullable_return_defined_body::side_global;
  if (n == 0) {
    nullable_return_defined_body::log_failure();
    return nullptr;
  }
  return &nullable_return_defined_body::the_session;
}

// ---------------- crossing the cap is not on its own a reason to inject ----------------
//
// Both of these return the address of a global and nothing else. No path can yield null, kept or
// dropped, so there is nothing for the cap to have hidden. The two differ only in whether they
// cross it: four ifs is 16 paths and under, five is 32 and over. Before the guard the second
// reported and the first did not, which made the caller's report a property of the callee's path
// count rather than of what it can return.

nullable_return_defined_body::Session* dangerous_under_cap(int a, int b, int c, int d) {
  int n = 0;
  if (a) { n++; }
  if (b) { n++; }
  if (c) { n++; }
  if (d) { n++; } // 2^4 paths, under the cap
  nullable_return_defined_body::sink_int(n);
  return &nullable_return_defined_body::the_session;
}

nullable_return_defined_body::Session* dangerous_over_cap(int a, int b, int c, int d, int e) {
  int n = 0;
  if (a) { n++; }
  if (b) { n++; }
  if (c) { n++; }
  if (d) { n++; }
  if (e) { n++; } // 2^5 paths, over the cap -- the only difference
  nullable_return_defined_body::sink_int(n);
  return &nullable_return_defined_body::the_session;
}

// ---------------- a path that stores nothing to the return variable ----------------
//
// Falling off the end of a non-void function writes nothing to the return variable. There is no
// store to find and so none to object to, which makes such a path invisible to "every store is
// non-null" -- and over the cap it is exactly the path Pulse drops, so the summary cannot be
// asked about it either. Both callers must report; the pair differs only in the path count, so
// if the under-cap one reported and the over-cap one did not, the cap would again be deciding.

nullable_return_defined_body::Session* dangerous_fall_under_cap(int a, int b, int c, int d) {
  int n = 0;
  if (a) { n++; }
  if (b) { n++; }
  if (c) { n++; }
  if (d) { n++; } // 2^4 paths, under the cap
  nullable_return_defined_body::sink_int(n);
  if (n > 0) {
    return &nullable_return_defined_body::the_session;
  }
  // no return here: the path falls off the end
}

nullable_return_defined_body::Session* dangerous_fall_over_cap(int a, int b, int c, int d, int e) {
  int n = 0;
  if (a) { n++; }
  if (b) { n++; }
  if (c) { n++; }
  if (d) { n++; }
  if (e) { n++; } // 2^5 paths, over the cap -- the only difference
  nullable_return_defined_body::sink_int(n);
  if (n > 0) {
    return &nullable_return_defined_body::the_session;
  }
  // no return here either
}

// ---------------- a field of an object the callee is given ----------------
//
// The list anchors on the START of the procedure name, so a method (Holder::dangerous_...) can
// never match the suite's `dangerous` pattern -- a method control would be inert. A free function
// taking the object exercises the same channel: a field write that has to survive applying the
// summary.

namespace nullable_return_defined_body {
struct Holder {
  int* field;
};
} // namespace nullable_return_defined_body

nullable_return_defined_body::Session* dangerous_fills_field(
    nullable_return_defined_body::Holder* h) {
  h->field = &nullable_return_defined_body::side_global;
  return &nullable_return_defined_body::the_session;
}

namespace nullable_return_defined_body {

// the list entry must survive the callee being at the disjunct cap
int capped_caller_bad() { return dangerous_capped(1, 1, 1, 1, 1)->level; }

// neither of these may report: the callee returns the address of a global on its only return
int under_cap_caller_ok() { return dangerous_under_cap(1, 1, 1, 1)->level; }

int over_cap_caller_ok() { return dangerous_over_cap(1, 1, 1, 1, 1)->level; }

// both of these must report, whichever side of the cap the callee is on
int fall_under_cap_caller_bad() { return dangerous_fall_under_cap(1, 1, 1, 1)->level; }

int fall_over_cap_caller_bad() { return dangerous_fall_over_cap(1, 1, 1, 1, 1)->level; }

// The null case is added to each surviving post of the capped summary, so it inherits that post's
// branch conditions. A caller with parameters of its own therefore gets a conditional -- latent --
// issue rather than a report here, and it surfaces one level up, where the arguments are concrete.
int capped_param_caller_latent(int a) { return dangerous_capped(a, 1, 1, 1, 1)->level; }

int capped_param_propagates_bad() { return capped_param_caller_latent(1); }

// A path the cap DROPPED is still missed: with these arguments the body really does return null,
// but no surviving post of the summary accepts them, so the call has no post to hang the null case
// on. Keeping the summary buys the callee's side effects at the price of this case; replacing the
// call would catch it and lose the side effects.
int FN_capped_dropped_path_bad() { return dangerous_capped(0, 0, 0, 0, 0)->level; }

// a capped callee's writes must still happen. Neither caller touches the RETURN value, so the
// null disjunct the cap earns must not make either of these report.
int capped_writes_global_caller_ok(int a, int b, int c, int d, int e) {
  side_global_ptr = nullptr;
  dangerous_capped_global(a, b, c, d, e);
  return *side_global_ptr;
}

// This one checks the VALUE written, not just that p became non-null: the unknown-call model
// havocs an out-parameter rather than leaving it alone, so a plain null check passes even when
// the summary was thrown away. Comparing against the address the callee writes does not.
int capped_outparam_caller_ok(int a, int b, int c, int d, int e) {
  int* p = nullptr;
  dangerous_capped_outparam(a, b, c, d, e, &p);
  if (p != &side_global) {
    int* q = nullptr;
    return *q; // unreachable once the callee's write is applied
  }
  return *p;
}

// the field write must still happen, or h.field stays null and we invent a report
int fills_field_caller_ok() {
  Holder h;
  h.field = nullptr;
  dangerous_fills_field(&h);
  return *h.field;
}

// an infeasible call: the guard contradicts itself, so the call is unreachable and nothing
// should be reported here whatever the callee does
int infeasible_call_ok(int x) {
  if (x > 0 && x < 0) {
    return dangerous_always_null()->level;
  }
  return 0;
}

} // namespace nullable_return_defined_body
