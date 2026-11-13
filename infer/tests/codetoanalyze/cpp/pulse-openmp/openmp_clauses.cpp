/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Data-sharing clauses. Translating a region where it is written models the clauses that act on
// the ORIGINAL variable (shared, lastprivate, reduction) but not the two that give the region
// its own copy, so private and firstprivate get one. Each case below says what the standard
// makes true, which is what the expectation encodes.

#include <stdlib.h>

void use(int);
void use_ptr(int*);

// private: the region gets a fresh, uninitialised copy and the write inside does not escape, so
// the variable really is uninitialised at the read after it.
void private_write_does_not_escape_bad() {
  int x;
#pragma omp parallel private(x)
  { x = 1; }
  use(x);
}

// private on a pointer: the allocation is stored into the copy, so nothing outside owns it.
void private_pointer_leaks_bad() {
  int* p = nullptr;
#pragma omp parallel private(p)
  { p = (int*)malloc(sizeof(int)); }
  use_ptr(p);
}

// firstprivate: the copy is initialised from the original, so the assignment before the region
// is read and is not a dead store.
void firstprivate_initialiser_is_live_ok() {
  int x = 7;
#pragma omp parallel firstprivate(x)
  { x = 1; }
  use(x);
}

// firstprivate, read inside the region. This is the shape that fails if the copy-in is dropped
// rather than merely misplaced: with no copy-in the read inside is uninitialised. A
// firstprivate case that only WRITES inside cannot tell the two apart.
void firstprivate_read_inside_ok() {
  int x = 7;
#pragma omp parallel firstprivate(x)
  { use(x); }
}

// shared: the region acts on the original, so the write escapes and the read after is fine.
void shared_write_escapes_ok() {
  int x;
#pragma omp parallel shared(x)
  { x = 1; }
  use(x);
}

// lastprivate: the last iteration's value is copied out, so the write escapes.
void lastprivate_copies_out_ok() {
  int x;
#pragma omp parallel for lastprivate(x)
  for (int i = 0; i < 4; i++) {
    x = i;
  }
  use(x);
}

// reduction: the partial results are combined back into the original.
void reduction_combines_out_ok() {
  int sum = 0;
#pragma omp parallel for reduction(+ : sum)
  for (int i = 0; i < 4; i++) {
    sum += i;
  }
  use(sum);
}

// A task makes the variables it references firstprivate implicitly. Copying a POINTER still
// leaves it pointing at the same memory, so freeing through the copy invalidates what the
// original points to and the access after the taskwait is a use-after-free.
void task_implicit_firstprivate_use_after_free_bad() {
  int* p = (int*)malloc(sizeof(int));
  if (p) {
#pragma omp task
    { free(p); }
#pragma omp taskwait
    *p = 1;
  }
}

// Two regions in one function must not share a copy: the second private(x) starts uninitialised
// again rather than seeing what the first one wrote, so the read inside it is uninitialised.
// Not reported: the region-local copy is a frontend temporary, and uninitialisedness is not
// tracked for those, so a read of a private variable before the region writes it is missed.
// The copy cannot simply be named after the variable instead -- two regions privatising the
// same name in one function would then share it, which is the very thing this case checks.
void FN_private_read_before_write_inside_bad() {
  int x;
#pragma omp parallel private(x)
  { x = 1; }
#pragma omp parallel private(x)
  { use(x); }
}

// A nested region privatising the same name: the inner copy shadows the outer one, and the
// outer variable is still untouched at the end.
void private_nested_same_name_bad() {
  int x;
#pragma omp parallel private(x)
  {
    x = 1;
#pragma omp parallel private(x)
    { x = 2; }
  }
  use(x);
}

// A region with no clause at all still behaves as before: the write escapes.
void no_clause_write_escapes_ok() {
  int x;
#pragma omp parallel
  { x = 1; }
  use(x);
}
