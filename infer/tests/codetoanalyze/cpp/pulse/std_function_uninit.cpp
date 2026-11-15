/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <functional>

// std::function and folly::Function share one model for their constructor and
// operator= (PulseModelsCpp's Function.assign). It represents the object by
// the callable stored at *this and never writes the object's own members, so
// those members keep the uninitialized marking the declaration gave them.
//
// That is invisible while every method that reads a member is modelled too.
// It becomes a false positive as soon as a read escapes the models, which is
// what the frontend's field-by-field copy of a `?:` branch temporary does.
// The folly::Function half of these shapes is in folly_function.cpp.
//
// The two _ok cases below are the std::function spelling of two of those
// shapes, and they are CANARIES, not regression pins: as of the commit that
// added them they report nothing, and nothing is what they reported on all
// four arms measured -- with and without the fix, crossed with and without
// the `std::function` entry in PulseUninitBlocklist. So neither the fix nor
// the blocklist is what keeps them quiet, and the reason is not established.
// The likeliest one is layout: libc++ holds the callable in an aligned-storage
// buffer plus a pointer, and fold_pointer_targets walks Tint/Tfloat/Tptr and
// Tstruct but skips Tarray, so there may be no scalar member to mark in the
// first place. That is a hypothesis, not a measurement. They are kept so that
// if std::function's modelling or layout changes and these start firing,
// something catches it.

namespace std_function_uninit {

using Fn = std::function<int()>;

struct TwoFunctions {
  Fn first;
  Fn second;
};

void sinkTwoFunctions(TwoFunctions t);
void sinkFunction(Fn f);

Fn functionFromLambda() {
  return [] { return 42; };
}

// An aggregate with std::function members moved into a `?:` temporary: the
// aggregate's implicit move constructor moves each member.
void cond_aggregate_move_ok(bool c, TwoFunctions* p) {
  sinkTwoFunctions(c ? TwoFunctions{} : static_cast<TwoFunctions&&>(*p));
}

// A std::function built from a lambda in one branch of a `?:`.
void cond_from_lambda_ok(bool c, Fn r) {
  sinkFunction(c ? static_cast<Fn&&>(r) : functionFromLambda());
}

// There is no std::function analogue of folly_function.cpp's third shape, a
// direct read of a member after the converting constructor: std::function's
// members are private and implementation-defined, so the only reads that reach
// them from outside are the compiler-generated ones above.

struct FunctionAndFlag {
  Fn callback;
  int flag;
};

// Control. The model initialises the std::function's own declared fields and
// must not reach anything else. Here the model runs on the callback member and
// a sibling of the enclosing struct is read without ever being written, so
// this must still report.
int sibling_field_read_uninitialized_bad() {
  FunctionAndFlag s;
  s.callback = [] { return 1; };
  return s.flag;
}

struct PlainPair {
  int a;
  int b;
};

// Control, for liveness: an ordinary uninitialized field read in this same
// translation unit must still report, so a silent file cannot be mistaken for
// a passing one.
int plain_field_read_uninitialized_bad() {
  PlainPair p;
  p.a = 1;
  return p.b;
}

} // namespace std_function_uninit
