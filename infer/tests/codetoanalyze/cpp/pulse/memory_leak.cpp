/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <stdlib.h>

void malloc_free_ok(int count) {
  int* e = (int*)malloc(count);
  int* res[] = {e};
  free(e);
}

void constant_index_ok(int count) {
  int* e = (int*)malloc(count);
  int* res[] = {e};
  free(res[0]);
}

void symbolic_index_ok(int x, int** res) {
  int* e = (int*)malloc(10);
  res[x] = e;
  free(res[x]);
}

// Two allocations made by one helper, reached through one call. They are two leaks, and the call
// that reaches them is the same for both, so it does not say which of them a report is about.
struct two_buffers {
  char* first;
  char* second;
};

two_buffers allocate_two() {
  char* first = (char*)malloc(8);
  char* second = (char*)malloc(16);
  return {first, second};
}

void two_inner_allocations_bad(bool b) {
  two_buffers buffers = allocate_two();
  if (b) {
    free(buffers.first);
  } else {
    free(buffers.second);
  }
}

void two_inner_allocations_ok() {
  two_buffers buffers = allocate_two();
  free(buffers.first);
  free(buffers.second);
}

// The same two leaks, where the one call and the one allocating line are shared as well, so that
// only the calls in between tell the two allocations apart.
char* allocate_leaf() { return (char*)malloc(8); }
char* allocate_through_first() { return allocate_leaf(); }
char* allocate_through_second() { return allocate_leaf(); }

two_buffers allocate_two_ways() {
  return {allocate_through_first(), allocate_through_second()};
}

void two_call_paths_bad(bool b) {
  two_buffers buffers = allocate_two_ways();
  if (b) {
    free(buffers.first);
  } else {
    free(buffers.second);
  }
}

// One allocation that is used and then becomes unreachable on two ways out of the procedure is
// one leak, reported once.
void one_allocation_two_exits_bad(bool b) {
  char* buffer = (char*)malloc(8);
  if (buffer == nullptr) {
    return;
  }
  if (b) {
    *buffer = 1;
    return;
  }
  *buffer = 2;
}

// One allocation that leaks on two ways out is one report, and it is reported at the EARLIEST of
// them, so that where the warning lands follows the source and not the order the paths happened
// to be analysed in. A reader who silences the warning does it above the first exit.
//
// The write before the branch is what makes this test say something. It is the last thing the
// path that does not take the branch does with the buffer, so that path's way out is the earlier
// one in the source and the later one to be analysed. Without it both ways out would be reached
// in source order and the test would pass whichever of them was picked.
void earliest_of_two_exits_bad(bool b) {
  char* buffer = (char*)malloc(8);
  if (buffer == nullptr) {
    return;
  }
  *buffer = 0;
  if (b) {
    *buffer = 1;
    return;
  }
}

// Three exits, so that picking the earliest cannot be confused with picking the first or the
// last one the analysis happens to produce.
void earliest_of_three_exits_bad(int n) {
  char* buffer = (char*)malloc(8);
  if (buffer == nullptr) {
    return;
  }
  *buffer = 0;
  if (n > 0) {
    *buffer = 1;
    if (n > 5) {
      *buffer = 2;
      return;
    }
    return;
  }
}

void earliest_of_two_exits_ok(bool b) {
  char* buffer = (char*)malloc(8);
  if (buffer == nullptr) {
    return;
  }
  *buffer = 0;
  if (b) {
    *buffer = 1;
    free(buffer);
    return;
  }
  free(buffer);
}
