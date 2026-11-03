/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Value-initialization of an aggregate. The language zero-initializes the whole object,
// recursively, when the default constructor is not user-provided, so reading any scalar
// leaf afterwards is defined. The paren spelling used to zero only the direct scalar
// members and leave the rest to the implicit constructor, which initializes nothing, so
// only a NESTED aggregate written as `T()` looked uninitialized -- the four other
// spellings, and a flat `S1()`, were already silent.
//
// The pair that keeps this honest is user_provided_ctor_bad / member_user_provided_ctor_ok
// at the bottom: they differ only in whether the default constructor is user-provided,
// which is exactly the condition the language uses to decide whether to zero-initialize,
// and so exactly the condition the translation is allowed to key on.

namespace value_init_paren {

struct Plain {
  int x;
  int y;
};
struct Nested {
  int x;
  Plain p;
};
struct Flat {
  int x;
};

// the five spellings that must all be silent
int paren_nested_ok() {
  Nested w = Nested();
  return w.x + w.p.y;
}

int brace_nested_ok() {
  Nested w = Nested{};
  return w.x + w.p.y;
}

int eq_brace_nested_ok() {
  Nested w = {};
  return w.x + w.p.y;
}

int direct_brace_nested_ok() {
  Nested w{};
  return w.x + w.p.y;
}

int paren_flat_ok() {
  Flat s = Flat();
  return s.x;
}

// reading a field not mentioned in a partial initializer is still defined
int partial_brace_ok() {
  Nested w = {1};
  return w.p.y;
}

// ------------------------- controls that must keep reporting -------------------------

// default-initialization initializes nothing
int default_init_nested_bad() {
  Nested w;
  return w.p.y;
}

// Not reported today, on this build or before this change: a default-initialized flat
// struct read through its own field. Recorded as a known limitation so the gap is
// visible rather than silently absent.
int FN_default_init_flat_bad() {
  Flat s;
  return s.x;
}

// A user-provided default constructor means the object is default-initialized, NOT
// zero-initialized, so a field the constructor forgets really is indeterminate.
struct UserCtor {
  int a;
  int b;
  UserCtor() : a(1) {}
};

int user_provided_ctor_bad() {
  UserCtor u = UserCtor();
  return u.b;
}

// ... whereas when the enclosing class's own default constructor is not user-provided,
// the whole object including this member is zero-initialized first and the member's
// constructor merely runs afterwards without touching b, so b reads 0.
struct Holder {
  int h;
  UserCtor u;
};

int member_user_provided_ctor_ok() {
  Holder w = Holder();
  return w.u.b;
}

} // namespace value_init_paren
