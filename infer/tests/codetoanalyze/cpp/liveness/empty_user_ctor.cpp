/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#ifndef EMPTY_CTOR_ACTIVE
#define EMPTY_CTOR_ACTIVE 0
#endif

namespace empty_user_ctor {

struct Owner {
  int value;
};

struct Guard {
  explicit Guard(Owner&) {}
};

int empty_guard_ok(Owner& owner) {
  Guard guard(owner);
  return owner.value;
}

struct Mixed {
  Mixed() = default;
  explicit Mixed(int) {}
};

void mixed_default_bad() { Mixed value; }

void mixed_provided_ok() { Mixed guard(4); }

struct ImplicitEmpty {};

void implicit_empty_bad() { ImplicitEmpty value; }

struct Defaulted {
  Defaulted() = default;
};

void first_defaulted_bad() { Defaulted value; }

struct DefinedLater {
  DefinedLater();
};

DefinedLater::DefinedLater() = default;

void later_defaulted_ok() { DefinedLater guard; }

struct Value {
  int value;
  explicit Value(int n) : value(n) {}
};

void member_initializer_bad(int n) { Value value(n); }

struct DefaultMember {
  int value = 42;
  explicit DefaultMember(Owner&) {}
};

void default_member_initializer_bad(Owner& owner) { DefaultMember value(owner); }

struct ValueBase {
  int value;
  ValueBase() : value(42) {}
};

struct InitializedBase : ValueBase {
  explicit InitializedBase(Owner&) {}
};

void base_initializer_bad(Owner& owner) { InitializedBase value(owner); }

struct TrivialBase {};

struct EmptyBase : TrivialBase {
  explicit EmptyBase(Owner&) {}
};

void trivial_base_ok(Owner& owner) { EmptyBase guard(owner); }

struct PlainMember {
  int value;
};

struct ZeroMember {
  PlainMember member;
  explicit ZeroMember(Owner&) : member() {}
};

void zero_member_initializer_bad(Owner& owner) { ZeroMember value(owner); }

struct Copy {
  Copy(const Copy&) {}
};

// Copy and move diagnostics remain value diagnostics even for empty bodies.
void explicit_copy_bad(const Copy& source) { Copy value(source); }

struct Move {
  Move(Move&&) {}
};

void explicit_move_bad(Move& source) { Move value(static_cast<Move&&>(source)); }

struct CopyWithDefault {
  CopyWithDefault(const CopyWithDefault&, int = 0) {}
};

void copy_with_default_bad(const CopyWithDefault& source) { CopyWithDefault value(source); }

struct NullStatement {
  explicit NullStatement(Owner&) { ; }
};

void null_statement_bad(Owner& owner) { NullStatement value(owner); }

struct ConditionalGuard {
  int value;
  explicit ConditionalGuard(Owner&) {
#if EMPTY_CTOR_ACTIVE
    value = 42;
#endif
  }
};

#if EMPTY_CTOR_ACTIVE
void conditional_guard_bad(Owner& owner) { ConditionalGuard value(owner); }
#else
void conditional_guard_ok(Owner& owner) { ConditionalGuard guard(owner); }
#endif

struct Cleanup {
  ~Cleanup() {}
};

void destructor_ok() { Cleanup guard; }

struct NestedCleanup {
  Cleanup member;
};

void member_destructor_ok() { NestedCleanup guard; }

int dead_scalar_bad(int n) {
  int scaled = n * 2;
  return 0;
}

int overwritten_scalar_bad(int n) {
  int value = n * 3;
  value = n + 4;
  return value;
}

// The baseline does not report a guard's overwritten assignment: its calls
// read the receiver. Pin that behavior rather than claiming a retained issue.
int overwritten_guard_ok(int n) {
  Value guard(n * 2);
  guard = Value(n + 4);
  return guard.value;
}

void overwritten_empty_guard_ok(Owner& owner) {
  Guard guard(owner);
  guard = Guard(owner);
}

} // namespace empty_user_ctor
