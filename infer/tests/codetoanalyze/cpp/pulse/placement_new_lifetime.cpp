/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <new>
#include <string>

namespace placement_new_lifetime {

struct Heavy {
  std::string s;
  Heavy() : s("x") {}
  ~Heavy() {}
};

// Placement new REUSES the storage it is given, so a destructor that ended the
// previous object's lifetime there must not make the new object's own
// constructor read as a use-after-delete. std::optional already applied this
// rule to its own storage; it is the general case of the same thing.
//
// THE RULE ONLY REACHES AS FAR AS THE CURRENT PROCEDURE. When the destruction
// and the reconstruction happen in the same function, the stale invalidation is
// in the state when placement new runs and is cleared. When they are split
// across functions, each is summarised separately and they only meet in the
// caller, where this model no longer runs -- see FP_cross_procedure_* below.

int buffer_reconstruct_ok() {
  alignas(Heavy) char buf[sizeof(Heavy)];
  Heavy* h = ::new (buf) Heavy();
  h->~Heavy();
  h = ::new (buf) Heavy();
  return (int)h->s.size();
}

template <class T>
struct Slot {
  union {
    T value_;
  };
  Slot() {}
  ~Slot() {}
  void destruct() { value_.~T(); }
  void construct() { ::new (&value_) T(); }
  T& get() { return value_; }
};

int union_reconstruct_ok() {
  Slot<Heavy> s;
  s.construct();
  s.value_.~Heavy();
  ::new (&s.value_) Heavy();
  return (int)s.value_.s.size();
}

// TRUE POSITIVES in the same procedure, where the revival above actually runs.
// These are the controls that catch it reviving too much: each one goes through
// a reconstruction and then commits a genuine double destruction or a read of
// freed storage, so over-revival silences them.
void buffer_reconstruct_then_double_destruct_bad() {
  alignas(Heavy) char buf[sizeof(Heavy)];
  Heavy* h = ::new (buf) Heavy();
  h->~Heavy();
  h = ::new (buf) Heavy();
  h->~Heavy();
  h->~Heavy();
}

void union_reconstruct_then_double_destruct_bad() {
  Slot<Heavy> s;
  s.construct();
  s.value_.~Heavy();
  ::new (&s.value_) Heavy();
  s.value_.~Heavy();
  s.value_.~Heavy();
}

void union_double_destruct_bad() {
  Slot<Heavy> s;
  s.construct();
  s.value_.~Heavy();
  s.value_.~Heavy();
}

int heap_delete_then_read_bad() {
  Slot<Heavy>* p = new Slot<Heavy>();
  p->construct();
  delete p;
  return (int)p->get().s.size();
}

// KNOWN LIMITATION, pinned so it is visible rather than forgotten. Splitting the
// destruction and the reconstruction across member functions puts them in
// different procedures: each is summarised on its own, the summaries only meet
// in the caller, and this model does not run there. Reviving across that
// boundary needs a summary-level "lifetime restart" effect, which Pulse has no
// way to express today (decision-pulse-lifetime-restart-summary). This is the
// unifex manual_lifetime shape.
int FP_cross_procedure_reconstruct_ok() {
  Slot<Heavy> s;
  s.construct();
  s.destruct();
  s.construct();
  return (int)s.get().s.size();
}

// The cross-procedure true positives keep reporting, as they did before.
void cross_procedure_double_destruct_bad() {
  Slot<Heavy> s;
  s.construct();
  s.destruct();
  s.destruct();
}

void cross_procedure_reconstruct_then_double_destruct_bad() {
  Slot<Heavy> s;
  s.construct();
  s.destruct();
  s.construct();
  s.destruct();
  s.destruct();
}

} // namespace placement_new_lifetime
