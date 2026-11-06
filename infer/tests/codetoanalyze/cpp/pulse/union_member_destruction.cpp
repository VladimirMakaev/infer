/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <new>

// A union's destructor destroys none of its variant members: the program has
// to destroy the active one itself. That is what a tagged union does, and it
// is why a union holding a non-trivially-destructible member must have a
// user-provided destructor -- a defaulted one is deleted. So this shape is
// not unusual; every hand-rolled tagged union has it.
namespace union_member_destruction {

struct Quiet {
  int n;
  Quiet(int v) : n(v) {}
  ~Quiet() {}
};

struct Loud {
  int m;
  Loud(int v) : m(v) {}
  ~Loud() {}
};

union QuietLoud {
  Quiet q;
  Loud l;
  char ch;
  QuietLoud() : ch(0) {}
  ~QuietLoud() {}
};

struct Tagged {
  QuietLoud u;
  bool is_q;

  Tagged(int x) : is_q(true) { new (&u.q) Quiet(x); }

  ~Tagged() {
    if (is_q) {
      u.q.~Quiet();
    } else {
      u.l.~Loud();
    }
  }

  void to_loud(int c) {
    if (is_q) {
      u.q.~Quiet();
    }
    new (&u.l) Loud(c);
    is_q = false;
  }
};

// Switch arms and read the arm that is now active. Correct code: the old
// member was destroyed by hand, the new one was constructed over it, and
// nothing reads the dead one.
int switch_arm_then_read_new_ok() {
  Tagged t(1);
  t.to_loud(2);
  return t.u.l.m;
}

// Reading the arm that was destroyed and never reconstructed is a use of a
// dead object.
int switch_arm_then_read_old_bad() {
  Tagged t(1);
  t.to_loud(2);
  return t.u.q.n;
}

int no_switch_read_active_ok() {
  Tagged t(1);
  return t.u.q.n;
}

// Destroy and reconstruct the same member.
int reset_same_member_ok() {
  Tagged t(1);
  t.u.q.~Quiet();
  new (&t.u.q) Quiet(5);
  return t.u.q.n;
}

// The same switch with no enclosing object: destroy one member, construct the
// other over it, read only the live one.
int destroy_member_then_new_sibling_ok() {
  QuietLoud u;
  new (&u.q) Quiet(1);
  u.q.~Quiet();
  new (&u.l) Loud(2);
  int r = u.l.m;
  u.l.~Loud();
  return r;
}

// Controls outside a union: the explicit destructor call must keep ending the
// object's lifetime. These fail if the union guard is written too broadly.
int explicit_destruct_then_read_bad() {
  Quiet q(1);
  q.~Quiet();
  return q.n;
}

int explicit_destruct_reconstruct_read_ok() {
  Quiet q(1);
  q.~Quiet();
  new (&q) Quiet(2);
  return q.n;
}

// A union member that owns memory, to show the guard does not hide a real
// use-after-delete: the program destroys the active member by hand, which
// frees, and reading it afterwards must still be reported.
struct Owner {
  int* p;
  Owner(int v) : p(new int(v)) {}
  ~Owner() { delete p; }
};

union OwnerBox {
  Owner o;
  char ch;
  OwnerBox() : ch(0) {}
  ~OwnerBox() {}
};

int owner_union_destroy_by_hand_ok() {
  OwnerBox b;
  new (&b.o) Owner(1);
  int v = *b.o.p;
  b.o.~Owner();
  return v;
}

int owner_union_destroy_then_read_bad() {
  OwnerBox b;
  new (&b.o) Owner(1);
  b.o.~Owner();
  return *b.o.p;
}

// A union template. The guard keys on the record being a union, and an
// instantiated union template is one, so switching the active member and
// reading the live one must be silent here too.
template <class A, class B>
union TUnion {
  A a;
  B b;
  char ch;
  TUnion() : ch(0) {}
  ~TUnion() {}
};

int template_union_switch_read_new_ok() {
  TUnion<Quiet, Loud> u;
  new (&u.a) Quiet(1);
  u.a.~Quiet();
  new (&u.b) Loud(2);
  int r = u.b.m;
  u.b.~Loud();
  return r;
}

int template_union_read_dead_bad() {
  TUnion<Quiet, Loud> u;
  new (&u.a) Quiet(1);
  u.a.~Quiet();
  new (&u.b) Loud(2);
  return u.a.n;
}

} // namespace union_member_destruction
