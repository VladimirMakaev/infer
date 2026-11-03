/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// The operand of sizeof is unevaluated, so a variable named only there has no read in
// the CFG and every store to it looks dead. That is what an assertion macro compiles
// to when assertions are off. The mention is a use AT ITS OWN PROGRAM POINT: it keeps
// the store before it alive and says nothing about a store that comes after.

namespace sizeof_only_read {

#define ASSERT_UNUSED(X) ((void)sizeof(X))

int f();
int g();
void use(int);

// The pattern the change is for: the only mention of the variable is unevaluated.
void assert_only_ok() {
  int result = f();
  ASSERT_UNUSED(result == 0);
}

void sizeof_only_ok() {
  int x = f();
  (void)sizeof(x);
}

// The GNU __alignof__ extension takes an expression, so a variable can appear in it.
// Standard alignof takes a type-id and cannot name one.
void alignof_only_ok() {
  int x = f();
  (void)__alignof__(x);
}

// Several variables in one unevaluated operand.
void sizeof_two_vars_ok() {
  int a = f();
  int b = g();
  (void)sizeof(a + b);
}

// TRUE POSITIVE: never mentioned at all.
void never_referenced_bad() { int x = f(); }

// TRUE POSITIVE: overwritten before any read.
void overwritten_bad() {
  int x = f();
  x = g();
  use(x);
}

// TRUE POSITIVE: a sizeof of a type names no variable, so nothing is recorded and the
// store is reported as it would be with no sizeof at all.
void sizeof_type_bad() {
  int x = f();
  (void)sizeof(int);
  (void)alignof(int);
}

// TRUE POSITIVE: the variable has an evaluated read, so the mention is not counted as a
// use and the store it follows is still reported. This is the case the per-variable form
// of this change got wrong.
void sizeof_then_read_bad() {
  int x = f();
  (void)sizeof(x);
  x = g();
  use(x);
}

// TRUE POSITIVE: the mention is a use where it stands and nowhere else, so the store
// AFTER it is judged exactly as it would be without the sizeof.
//
// Known limitation, deliberate: the FIRST store is dead on its own terms -- `x = g()`
// overwrites it before any read -- and is no longer reported, because the mention
// between them counts as a use of the value it wrote. That is the whole point of the
// change: an assertion macro refers to the value, so a store it follows is not a value
// someone forgot to use. Distinguishing "the macro refers to this value" from "the macro
// happens to name this variable" needs more than the variable's name.
void overwritten_then_sizeof_only_bad() {
  int x = f();
  (void)sizeof(x);
  x = g();
}

// TRUE POSITIVE: a store after the mention, with the mention reading nothing before it.
// The mention must not protect a store that did not precede it.
void sizeof_before_store_bad() {
  int x;
  (void)sizeof(x);
  x = f();
}

// TRUE POSITIVE: a second local in the same procedure is genuinely dead; silencing the
// first must not silence this one.
void one_unevaluated_one_dead_bad() {
  int kept = f();
  (void)sizeof(kept);
  int dead = g();
}

// Read normally as well as in sizeof: silent before and after the change.
void read_and_sizeof_ok() {
  int x = f();
  (void)sizeof(x);
  use(x);
}

// KNOWN LIMITATION: the variable is named in a TYPE operand, not an expression, so it is
// not recorded and the store is still reported. Conservative: nothing is silenced that
// was not silenced before.
void FP_decltype_operand_ok() {
  int x = f();
  (void)sizeof(decltype(x));
}

// KNOWN LIMITATION: noexcept is a different unevaluated context and is not recorded.
void FP_noexcept_operand_ok() {
  int x = f();
  (void)noexcept(x == 0);
}

// KNOWN LIMITATION: the operand of a static_assert is not translated at all, so the
// mention never reaches the recording and the store is still reported.
void FP_static_assert_operand_ok() {
  int x = f();
  static_assert(sizeof(x) > 0, "");
}

// TRUE POSITIVE: two different locals of the same name declared on one line, the second
// of them genuinely dead. They are separated by the declaration column, so the sizeof
// mention of the first does not silence the second.
void same_line_same_name_bad() {
  { int x = f(); ASSERT_UNUSED(x); } { int x = g(); }
}

// TRUE POSITIVE x2: the same two locals with no unevaluated mention at all. Both stores
// are dead and both are reported, one per declaration.
void same_line_same_name_both_dead_bad() {
  { int x = f(); } { int x = g(); }
}

} // namespace sizeof_only_read
