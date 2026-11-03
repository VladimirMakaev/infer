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
// THE RULE ALSO HAS TO CROSS A CALL. The storage a callee placement-news into
// usually holds none of the destroyed object's subobjects in the CALLEE's heap:
// they are in the caller's. Abducing them would make the callee demand that the
// previous occupant still be there and be valid, which is the opposite of what
// placement new means, and every caller that destroyed it first -- the whole
// point of the idiom -- would fail that precondition. So the storage is marked
// instead, and the mark travels in the summary.

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

// ACROSS A CALL. This is the unifex manual_lifetime shape: the placement new is
// in a callee of its own, and the destruction is somewhere else entirely.
int cross_procedure_reconstruct_ok() {
  Slot<Heavy> s;
  s.construct();
  s.destruct();
  s.construct();
  return (int)s.get().s.size();
}

// Only ONE of the two sides needs to be in a callee for the summaries to have to
// carry the fact; both halves are pinned so a fix that only handles one shows up.
int destruct_here_construct_in_callee_ok() {
  Slot<Heavy> s;
  s.construct();
  s.value_.~Heavy();
  s.construct();
  return (int)s.get().s.size();
}

int destruct_in_callee_construct_here_ok() {
  Slot<Heavy> s;
  s.construct();
  s.destruct();
  ::new (&s.value_) Heavy();
  return (int)s.value_.s.size();
}

// Reading the storage DIRECTLY afterwards, rather than through another callee:
// nothing of the callee's post overwrites the caller's own cell here.
int cross_procedure_reconstruct_direct_read_ok() {
  Slot<Heavy> s;
  s.construct();
  s.destruct();
  s.construct();
  return (int)s.value_.s.size();
}

// A plain buffer, so this is not about unions.
struct Buffer {
  alignas(Heavy) char b[sizeof(Heavy)];
  Heavy* construct() { return ::new (b) Heavy(); }
};

int cross_procedure_buffer_reconstruct_ok() {
  Buffer buf;
  Heavy* h = buf.construct();
  h->~Heavy();
  h = buf.construct();
  return (int)h->s.size();
}

// The fact has to keep travelling: the real chain is several calls deep.
struct Nested {
  Slot<Heavy> slot;
  void reconstruct() {
    slot.destruct();
    slot.construct();
  }
};

int nested_reconstruct_ok() {
  Nested n;
  n.slot.construct();
  n.reconstruct();
  return (int)n.slot.get().s.size();
}

// A member with no destructor of its own, so the read is of a plain field rather
// than of a modelled std::string.
struct Plain {
  int n;
  Plain() : n(1) {}
  ~Plain() {}
};

int cross_procedure_plain_field_ok() {
  Slot<Plain> s;
  s.construct();
  s.destruct();
  s.construct();
  return s.get().n;
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

// KNOWN LIMITATION, AND IT IS OLDER AND WIDER THAN THIS CHANGE: reading an object
// that was destroyed in place and NOT reconstructed is not reported, through a
// callee or inline, for a union member or a plain buffer. Only `delete` followed
// by a use is (heap_delete_then_read_bad above). Both of these are silent on the
// parent commit too, so nothing here revived them; they are pinned because they
// are the shapes a reader of this file will expect to be covered, and because
// they are the controls that would catch the mark outliving what it describes --
// in the first one the LAST thing to happen to the storage is a destruction.
int FN_cross_procedure_reconstruct_then_destruct_then_read() {
  Slot<Heavy> s;
  s.construct();
  s.destruct();
  s.construct();
  s.destruct();
  return (int)s.get().s.size();
}

int FN_cross_procedure_destruct_then_read() {
  Slot<Heavy> s;
  s.construct();
  s.destruct();
  return (int)s.get().s.size();
}

} // namespace placement_new_lifetime
