/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstdlib>

namespace call_aliasing_summary {

struct Node {
  int v;
  int arr[8];
  Node* next;
};

static int sink = 0;

static int two_cells(int* q, int k) {
  q[k & 7] = 1;
  q[0] = 2;
  return sink + k;
}

// The two writes are to DIFFERENT cells, so nothing aliases and the summary
// applies: the null dereference after the call is found.
void distinct_cells_bad() {
  int* a0 = new int[8];
  sink += two_cells(a0, 1);
  Node* b0 = nullptr;
  sink += b0->v;
}

// The two writes are to the SAME cell. Applying the summary makes one caller
// address stand for two of the callee's, and the alias cannot be expressed as
// a specialization. The call is treated as unknown instead of discarding every
// disjunct, so the rest of the procedure is still analysed and the null
// dereference is found.
void coinciding_cells_bad() {
  int* a0 = new int[8];
  sink += two_cells(a0, 0);
  Node* b0 = nullptr;
  sink += b0->v;
}

static int array_and_object(int* q, Node* p, int k) {
  p = p->next;
  q[k & 7] = 1;
  p->arr[k & 7] = 2;
  return sink + k;
}

// The array argument is its own array, so it does not alias the object's field.
void separate_array_bad() {
  Node* n0 = new Node();
  n0->next = n0;
  int* a0 = new int[8];
  sink += array_and_object(a0, n0, 0);
  Node* b0 = nullptr;
  sink += b0->v;
}

// The array argument IS the object's field, reached again inside the callee
// through a field that points back to the object, and both are read. Same
// situation as above: the call is treated as unknown rather than killing the
// caller's state.
void shared_array_bad() {
  Node* n0 = new Node();
  n0->next = n0;
  int* a0 = n0->arr;
  sink += array_and_object(a0, n0, 0);
  Node* b0 = nullptr;
  sink += b0->v;
}

static int one_read(int* q, Node* p, int k) {
  p = p->next;
  q[k & 7] = 1;
  return sink + k;
}

// The same aliasing with only one read in the callee: nothing is lost here even
// before the fix, and it must keep reporting.
void shared_array_one_read_bad() {
  Node* n0 = new Node();
  n0->next = n0;
  int* a0 = n0->arr;
  sink += one_read(a0, n0, 0);
  Node* b0 = nullptr;
  sink += b0->v;
}

// Control: a null dereference with no call at all.
void no_call_bad() {
  Node* b0 = nullptr;
  sink += b0->v;
}

// Control: a call whose summary applies cleanly must not start reporting
// anything of its own.
void clean_call_ok() {
  int* a0 = new int[8];
  sink += two_cells(a0, 1);
  delete[] a0;
}

} // namespace call_aliasing_summary
