/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <stdlib.h>

struct Fields {
  int* target;
  int f0;
  int* p0;
  int f1;
  int* p1;
  int f2;
  int* p2;
  int f3;
  int* p3;
};

// 16 paths with their own preconditions: the summary is bigger than the limit
void many_paths(struct Fields* s) {
  *(s->target) = 0;
  if (s->f0) {
    s->p0 = NULL;
  } else {
    *(s->p0) = 0;
  }
  if (s->f1) {
    s->p1 = NULL;
  } else {
    *(s->p1) = 1;
  }
  if (s->f2) {
    s->p2 = NULL;
  } else {
    *(s->p2) = 2;
  }
  if (s->f3) {
    s->p3 = NULL;
  } else {
    *(s->p3) = 3;
  }
}

// the summary of [many_paths] is not stored, so the call is unknown
void FN_call_many_paths_bad() {
  struct Fields s = {NULL};
  many_paths(&s);
}

int deref(int* p) { return *p; }

int call_deref_bad() { return deref(NULL); }
