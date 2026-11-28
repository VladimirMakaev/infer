/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Where the cheap-to-copy decision sits for small types.
//
// A copy is only visible here when the type is not trivially copyable: for a
// trivially copyable one no copy constructor call is emitted, so every function
// below would be silent for a reason that has nothing to do with the width.
// Each type is therefore non-trivially copyable: five of them by declaring a
// destructor, UserCopyPtr by its own copy constructor.
//
// These cases pin the current boundary rather than argue for one. The width walk
// gives up on a field it cannot size, so a type holding a pointer is not "small"
// whatever its real width, while a pointer-free type of the same width is. A
// change to that walk, or to the cheap-to-copy test, moves these cases, which is
// the point of having them.
//
// The ownership half of the boundary is use_after_destructor::S::reinit: a
// 64-bit type with one raw owning pointer, a user destructor that deletes it and
// no user copy constructor. Advising a reference there is what prevents a double
// delete, so that row must survive any widening of the cheap-to-copy test.

namespace pointer_width {

struct Inner {
  int* p;
};

struct ImplicitPtr {
  int* p;
  ~ImplicitPtr();
};

struct ImplicitInt {
  long x;
  ~ImplicitInt();
};

struct ImplicitNested {
  Inner inner;
  ~ImplicitNested();
};

struct ImplicitWide {
  double a, b, c, d, e, f, g, h;
  ~ImplicitWide();
};

struct ImplicitWideWithPtr {
  int* p;
  double a, b, c, d, e, f, g, h;
  ~ImplicitWideWithPtr();
};

struct UserCopyPtr {
  int* p;
  UserCopyPtr();
  UserCopyPtr(const UserCopyPtr& other) : p(other.p) {}
};

void consume_implicit_ptr(ImplicitPtr);
void consume_implicit_int(ImplicitInt);
void consume_implicit_nested(ImplicitNested);
void consume_implicit_wide(ImplicitWide);
void consume_implicit_wide_with_ptr(ImplicitWideWithPtr);
void consume_user_copy_ptr(UserCopyPtr);

// One pointer and nothing else.
void forward_pointer_field_bad(ImplicitPtr x) { consume_implicit_ptr(x); }

// The pointer is a field of a field: the width walk recurses into member structs.
void forward_nested_pointer_field_bad(ImplicitNested x) {
  consume_implicit_nested(x);
}

// Same width as ImplicitPtr with no pointer in it, so the size escape applies.
void forward_integer_field_ok(ImplicitInt x) { consume_implicit_int(x); }

// Well over the threshold and pointer-free.
void forward_wide_ok(ImplicitWide x) { consume_implicit_wide(x); }

// Over the threshold and carrying a pointer.
void forward_wide_with_pointer_bad(ImplicitWideWithPtr x) {
  consume_implicit_wide_with_ptr(x);
}

// A copy constructor written by hand is free to follow the pointer, so the width
// of this type says nothing about what copying it costs.
void forward_user_copy_pointer_bad(UserCopyPtr x) { consume_user_copy_ptr(x); }

} // namespace pointer_width
