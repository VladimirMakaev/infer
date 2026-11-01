/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// no std::make_unique here: this file is also compiled with -std=c++11 by the pulse-11 case

#include <memory>
#include <utility>

namespace unknown_call_ownership {

struct Thing {
  int v;
};

// none of these has a body in this translation unit
void sink_by_value(std::unique_ptr<Thing> t);
void borrow_unique_const_ref(const std::unique_ptr<Thing>& t);
void borrow_unique_ref(std::unique_ptr<Thing>& t);
void steal_unique_rvalue_ref(std::unique_ptr<Thing>&& t);
void read_through_unique_ptr(std::unique_ptr<Thing>* t);
void borrow_const(const Thing* t);
void take_mut(Thing* t);

// A by-value std::unique_ptr parameter is an ownership transfer by the type system: the callee's
// own parameter object owns the pointee and is destroyed with the callee's frame, whatever the
// callee does with it.
void sink_temporary_ok() { sink_by_value(std::unique_ptr<Thing>(new Thing())); }

void sink_moved_local_ok() {
  std::unique_ptr<Thing> t(new Thing());
  sink_by_value(std::move(t));
}

// KNOWN FALSE POSITIVE, still reported. The wrapper's own by-value parameter is move-constructed
// into the temporary that sink_by_value receives, which empties the formal, so the value whose
// ownership is released is no longer reachable from a formal in the wrapper's post and the
// summary has no way to tell the caller about it.
void visible_wrapper(std::unique_ptr<Thing> t) { sink_by_value(std::move(t)); }

void FP_sink_through_wrapper_ok() {
  visible_wrapper(std::unique_ptr<Thing>(new Thing()));
}

// A REFERENCE parameter is not a transfer. Each of these is silent for a different, older reason
// -- the caller's own unique_ptr destructor frees the pointee -- and must stay silent.
void borrow_const_ref_ok() {
  std::unique_ptr<Thing> t(new Thing());
  borrow_unique_const_ref(t);
}

void borrow_ref_ok() {
  std::unique_ptr<Thing> t(new Thing());
  borrow_unique_ref(t);
}

void steal_rvalue_ref_ok() {
  steal_unique_rvalue_ref(std::unique_ptr<Thing>(new Thing()));
}

// A pointer TO a unique_ptr is not a by-value parameter: the formal is a Pk_pointer, so the
// transfer must not fire. It is silent anyway because the local is destroyed.
void pointer_to_unique_ok() {
  std::unique_ptr<Thing> t(new Thing());
  read_through_unique_ptr(&t);
}

// ------------------------- true-positive controls -------------------------

// The callee is VISIBLE and only borrows, so this procedure still owns the Thing and leaks it.
int peek(const Thing* t) { return t->v; }

int borrow_known_const_bad() {
  Thing* t = new Thing();
  return peek(t);
}

int borrow_known_ref_bad() {
  Thing* t = new Thing();
  const Thing& r = *t;
  return r.v;
}

// A raw pointer handed to a callee with no body as a pointer to const. Const-ness does not stop
// the callee from adopting the pointer, but it does not say that it did either, so this is still
// reported: a borrow and a transfer look the same from here.
void const_ptr_to_unknown_bad() { borrow_const(new Thing()); }

// The callee with no body gets a pointer to const and we go on using the Thing afterwards and
// drop it: a leak whichever way the callee is read.
int const_borrowed_by_unknown_then_used_bad() {
  Thing* t = new Thing();
  borrow_const(t);
  return t->v;
}

// the plain leak, with nothing passed anywhere. Written without a `(void)t;`, which is an
// unknown call on a non-const pointer and lets the allocation escape all by itself.
void plain_leak_bad() { new Thing(); }

// a non-const pointer escapes to a callee with no body: unchanged by this commit
void mut_ptr_to_unknown_ok() { take_mut(new Thing()); }

} // namespace unknown_call_ownership
