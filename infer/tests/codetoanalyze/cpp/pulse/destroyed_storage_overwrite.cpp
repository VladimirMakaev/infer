/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
// Overwriting the storage of an object destroyed in place. The storage outlives
// the object and may be reused, so a byte-wise write through a `void *` is not
// an access to the object that was there. Storage that is null, freed, deleted
// or out of scope is a different matter: there is nothing to write into, and
// those still report.
#include <cstdlib>
#include <cstring>
#include <new>

namespace destroyed_storage_overwrite {

struct Occupant {
  int tag;
  int* payload;
  Occupant() : tag(0), payload(nullptr) {}
  ~Occupant() {}
};

// ---------------------------------------------- the storage is still there: no
// report

void memcpy_into_destroyed_storage_ok(Occupant* o, const void* src) {
  o->~Occupant();
  std::memcpy(static_cast<void*>(o), src, sizeof(Occupant));
}

void memmove_into_destroyed_storage_ok(Occupant* o, const void* src) {
  o->~Occupant();
  std::memmove(static_cast<void*>(o), src, sizeof(Occupant));
}

void memset_destroyed_storage_ok(Occupant* o) {
  o->~Occupant();
  std::memset(static_cast<void*>(o), 0, sizeof(Occupant));
}

// The whole reuse idiom the standard prescribes: end the lifetime, clear the
// storage, start a new lifetime in it, then read the new occupant.
int clear_then_rebuild_ok(Occupant* o) {
  o->~Occupant();
  std::memset(static_cast<void*>(o), 0, sizeof(Occupant));
  new (o) Occupant();
  return o->tag;
}

// Compacting an array: the destroyed elements are the destination, the
// surviving ones the source.
void close_the_gap_ok(Occupant* first, Occupant* last, Occupant* end) {
  for (Occupant* it = first; it != last; ++it) {
    it->~Occupant();
  }
  std::memmove(static_cast<void*>(first),
               static_cast<const void*>(last),
               static_cast<unsigned long>(end - last) * sizeof(Occupant));
}

// ---------------------------------------------- there is no storage to write
// into: still report

void memcpy_into_freed_storage_bad(const void* src) {
  void* p = malloc(sizeof(Occupant));
  if (p == nullptr) {
    return;
  }
  free(p);
  std::memcpy(p, src, sizeof(Occupant));
}

void memcpy_into_deleted_storage_bad(const void* src) {
  Occupant* o = new Occupant();
  delete o;
  std::memcpy(static_cast<void*>(o), src, sizeof(Occupant));
}

void memset_freed_storage_bad() {
  void* p = malloc(sizeof(Occupant));
  if (p == nullptr) {
    return;
  }
  free(p);
  std::memset(p, 0, sizeof(Occupant));
}

// returning the address of a local is its own error, and the fixture records it
// so the out-of-scope twin below has somewhere to get dead storage from
Occupant* escaped_storage_bad() {
  Occupant local;
  return &local;
}

void memcpy_into_out_of_scope_storage_bad(const void* src) {
  Occupant* gone = escaped_storage_bad();
  std::memcpy(static_cast<void*>(gone), src, sizeof(Occupant));
}

// Null is not storage either, and the header above says so: these hold it to
// it.
void memcpy_into_null_bad(const void* src) {
  Occupant* o = nullptr;
  std::memcpy(static_cast<void*>(o), src, sizeof(Occupant));
}

void memset_null_bad() {
  Occupant* o = nullptr;
  std::memset(static_cast<void*>(o), 0, sizeof(Occupant));
}

// ---------------------------------------------- the other size path

// A copy of exactly `sizeof(T)` is written cell by cell; any other size
// overwrites the contents wholesale. Every case above takes the first path, so
// these take the second: which one a call lands in must not decide whether it
// reports.

void memcpy_nonexact_into_destroyed_storage_ok(Occupant* o, const void* src) {
  o->~Occupant();
  std::memcpy(static_cast<void*>(o), src, 2 * sizeof(Occupant));
}

void memcpy_unknown_size_into_destroyed_storage_ok(Occupant* o,
                                                   const void* src,
                                                   unsigned long n) {
  o->~Occupant();
  std::memcpy(static_cast<void*>(o), src, n);
}

// ...and the cross product, which is the case that actually pins it: the other
// size path AND storage that is gone. Either half on its own passes whichever
// way this is wired.
void memcpy_nonexact_into_freed_storage_bad(const void* src) {
  void* p = malloc(sizeof(Occupant));
  if (p == nullptr) {
    return;
  }
  free(p);
  std::memcpy(p, src, 2 * sizeof(Occupant));
}

void memset_unknown_size_freed_storage_bad(unsigned long n) {
  void* p = malloc(sizeof(Occupant));
  if (p == nullptr) {
    return;
  }
  free(p);
  std::memset(p, 0, n);
}

// ---------------------------------------------- the object, not the storage:
// still report

int read_member_after_destroy_bad(Occupant* o) {
  o->~Occupant();
  return o->tag;
}

void write_member_after_destroy_bad(Occupant* o) {
  o->~Occupant();
  o->payload = nullptr;
}

// ---------------------------------------------- across a call boundary

// Only one validity requirement is kept per address and the first one recorded
// wins, so a callee that writes bytes and then reads the object needs the
// stronger of the two to survive into its summary. These cases are what hold
// that: without it the write would excuse the read for every caller, and only
// the order of two statements inside the callee would decide.

void callee_overwrites_storage(Occupant* o, const void* src) {
  std::memcpy(static_cast<void*>(o), src, sizeof(Occupant));
}

// the tolerance has to survive a call, or the whole point is lost
void destroyed_then_callee_overwrites_ok(Occupant* o, const void* src) {
  o->~Occupant();
  callee_overwrites_storage(o, src);
}

// ...while storage that is gone still reports through that same callee
void freed_then_callee_overwrites_bad(const void* src) {
  void* p = malloc(sizeof(Occupant));
  if (p == nullptr) {
    return;
  }
  free(p);
  callee_overwrites_storage(static_cast<Occupant*>(p), src);
}

int callee_overwrites_then_reads(Occupant* o, const void* src) {
  std::memcpy(static_cast<void*>(o), src, sizeof(Occupant));
  return o->tag;
}

// the case above is the one that needs the stronger requirement: the storage is
// overwritten and then read as an object that no lifetime restarted
int destroyed_then_callee_overwrites_and_reads_bad(Occupant* o,
                                                   const void* src) {
  o->~Occupant();
  return callee_overwrites_then_reads(o, src);
}

// the same through a pointer member that is dereferenced, so the consequence is
// concrete
int callee_overwrites_then_derefs(Occupant* o, const void* src) {
  std::memcpy(static_cast<void*>(o), src, sizeof(Occupant));
  return *(o->payload);
}

int destroyed_then_callee_overwrites_and_derefs_bad(Occupant* o,
                                                    const void* src) {
  o->~Occupant();
  return callee_overwrites_then_derefs(o, src);
}

// reading before the write reports whichever requirement wins, so this one is
// the control that says the case above is about the order and not about calls
// in general
int callee_reads_then_overwrites(Occupant* o, const void* src) {
  int t = o->tag;
  std::memcpy(static_cast<void*>(o), src, sizeof(Occupant));
  return t;
}

int destroyed_then_callee_reads_then_overwrites_bad(Occupant* o,
                                                    const void* src) {
  o->~Occupant();
  return callee_reads_then_overwrites(o, src);
}

// writing one object and reading another keeps the two apart
int callee_overwrites_a_reads_b(Occupant* a, Occupant* b, const void* src) {
  std::memcpy(static_cast<void*>(a), src, sizeof(Occupant));
  return b->tag;
}

int destroyed_b_callee_overwrites_a_reads_b_bad(Occupant* a,
                                                Occupant* b,
                                                const void* src) {
  b->~Occupant();
  return callee_overwrites_a_reads_b(a, b, src);
}

// The reuse idiom behind a call: clear the storage, start a new lifetime in it,
// read the new occupant. This differs from `callee_overwrites_then_reads` by
// one line, and that line is what makes the difference, so the two together are
// the control for the rule above.
int callee_overwrites_rebuilds_and_reads(Occupant* o, const void* src) {
  std::memcpy(static_cast<void*>(o), src, sizeof(Occupant));
  new (o) Occupant();
  return o->tag;
}

int destroyed_then_callee_rebuilds_ok(Occupant* o, const void* src) {
  o->~Occupant();
  return callee_overwrites_rebuilds_and_reads(o, src);
}

// ---------------------------------------------- the deliberate remainder

// Reading the bytes of storage whose occupant was destroyed is as legal as
// writing them, but no idiom needs it and copying a dead object's
// representation out is more often a mistake than not, so the source of a copy
// is checked as an ordinary pointer.
void FP_memcpy_from_destroyed_storage_ok(Occupant* o, void* dest) {
  o->~Occupant();
  std::memcpy(dest, static_cast<const void*>(o), sizeof(Occupant));
}

// A copy into the storage of a type like this one starts a new object there by
// itself, so the read that follows is of a live object and the shape is legal
// all the way through. Telling it apart from
// `destroyed_then_callee_overwrites_and_reads_bad` needs the destructor of the
// type, which is not consulted here, and the stronger requirement is the safer
// default of the two.
struct Bytes {
  int a;
  int b;
};

int callee_overwrites_bytes_then_reads(Bytes* p, const void* src) {
  std::memcpy(static_cast<void*>(p), src, sizeof(Bytes));
  return p->a;
}

int FP_pod_destroyed_then_callee_overwrites_and_reads_ok(Bytes* p,
                                                         const void* src) {
  p->~Bytes();
  return callee_overwrites_bytes_then_reads(p, src);
}

} // namespace destroyed_storage_overwrite
