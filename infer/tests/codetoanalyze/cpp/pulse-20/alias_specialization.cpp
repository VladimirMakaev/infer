/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstddef>
#include <new>
#include <span>

// Two aliased formals where only one of them is invalidated, for the invalidations
// that C has no way to write. `codetoanalyze/c/pulse/specialization.c` covers `free`;
// the question these add is which OTHER invalidations make a callee value one whose
// identity the caller can observe.

namespace alias_specialization {

struct Cell {
  int value;
  // user-provided, so `p->~Cell()` is a destructor call the frontend emits rather
  // than nothing at all
  ~Cell() {}
};

// An explicit `a->~Cell()` evaluates its receiver with `NoAccess`, exactly as `free`
// does, so `a` carries an invalidation and has no cell in the callee's heap. With the
// same pointer passed twice the read through `b` is a use after the object's lifetime
// ended, and seeing it needs the alias to be recognised.
int destroy_first_read_second(Cell* a, Cell* b) {
  a->~Cell();
  return b->value;
}

// Unlike the `free` cases this one does not fault under memcheck and cannot: destroying
// an object in place leaves its storage alone, which is the whole difference between
// `CppDestructedInPlace` and `CppDelete`. The read is still reading a dead object.
int call_destroy_first_read_second_bad() {
  alignas(Cell) unsigned char storage[sizeof(Cell)];
  Cell* c = new (storage) Cell{1};
  return destroy_first_read_second(c, c);
}

// CONTROL: distinct objects, so there is no alias and nothing to report. Reporting here
// would mean the alias test fires on an invalidation alone rather than on aliasing.
int call_destroy_first_read_second_distinct_ok() {
  alignas(Cell) unsigned char storage_c[sizeof(Cell)];
  alignas(Cell) unsigned char storage_d[sizeof(Cell)];
  Cell* c = new (storage_c) Cell{1};
  Cell* d = new (storage_d) Cell{2};
  int v = destroy_first_read_second(c, d);
  d->~Cell();
  return v;
}

// CONTROL for an invalidation that must NOT count: leaving the bounds of a view says
// where a value points, not that anything's lifetime ended, and the pointer the caller
// passed is still perfectly good. `subspan` past the end is the bug here and is reported
// on its own line; the read through `b` is valid and must stay unreported even though
// the two formals alias.
//
// Today this holds for a second reason as well, which is worth writing down because it
// makes the case weaker than it looks: both span models invalidate a value they have
// just made up rather than the pointer they were given, so an out-of-bounds attribute
// never lands on a formal at all. The case guards the choice rather than demonstrating
// it.
// The order matters. The subspan creates the invalidation but reports nothing on its own, so
// `*b` is read while the out-of-bounds attribute is live -- that is the moment under test --
// and the view is only read afterwards, which is what makes the out-of-bounds visible. A
// version that discarded the subspan compiled, ran, and reported nothing at all, which would
// have proved only that the fixture was silent.
int span_out_of_bounds_then_read_bad(int* a, int* b) {
  std::span<int> s(a, 1);
  std::span<int> t = s.subspan(2);
  int second = *b;
  return second + t[0];
}

int call_span_out_of_bounds_then_read_bad() {
  int v[1] = {3};
  return span_out_of_bounds_then_read_bad(v, v);
}

} // namespace alias_specialization
