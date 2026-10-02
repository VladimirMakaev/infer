/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <stdlib.h>
#include <string.h>

struct X {
  int f;
};

typedef struct X X;

void memcpy_ok() {
  X x;
  X* p = malloc(sizeof(X));
  if (p)
    memcpy(p, &x, sizeof(X));
  free(p);
}

void memcpy_to_null_bad() {
  X x;
  X* p = NULL;
  memcpy(p, &x, sizeof(X)); // crash
}

void memcpy_to_null_indirect_bad() {
  X x;
  X* r;
  X* p = NULL;
  r = p;
  memcpy(r, &x, sizeof(X)); // crash
}

void memcpy_from_null_bad() {
  X* src = NULL;
  X* p = malloc(sizeof(X));
  if (p) {
    memcpy(p, src, sizeof(X)); // crash
    free(p);
  }
}

struct Y {
  int* p;
};

int memset_memcpy_ok(const struct Y* src) {
  struct Y y;
  memset(&y, 0, sizeof(struct Y));
  memcpy(&y, src, sizeof(struct Y));
  return *y.p;
}

void memcpy_overwrites_pointer_leak_bad(const struct Y* src) {
  struct Y y;
  y.p = malloc(sizeof(int));
  memcpy(&y, src, sizeof(struct Y));
  free(y.p);
}

void swap_y(struct Y* a, struct Y* b) {
  struct Y tmp;
  memcpy(&tmp, a, sizeof(struct Y));
  memcpy(a, b, sizeof(struct Y));
  memcpy(b, &tmp, sizeof(struct Y));
}

void memcpy_swap_ok() {
  struct Y a, b;
  a.p = malloc(sizeof(int));
  b.p = malloc(sizeof(int));
  swap_y(&a, &b);
  free(a.p);
  free(b.p);
}

int memcpy_copies_null_bad() {
  struct Y src, dst;
  src.p = NULL;
  memcpy(&dst, &src, sizeof(struct Y));
  return *dst.p;
}

struct Z {
  struct X x;
  int* p;
};

void memcpy_first_field_keeps_pointer_ok(const struct X* src) {
  struct Z z;
  z.p = malloc(sizeof(int));
  memcpy(&z, src, sizeof(struct X));
  free(z.p);
}

void memcpy_first_field_aliases_object_ok(struct Z* z, const struct X* src) {
  if ((void*)&z->x == (void*)z) {
    memcpy(z, src, sizeof(struct X));
  }
}

struct W {
  struct Y y;
  int n;
};

void memcpy_first_field_aliases_object_keeps_pointer_ok(struct W* w) {
  if ((void*)&w->y != (void*)w) {
    return;
  }
  struct W tmp;
  tmp.y.p = malloc(sizeof(int));
  tmp.n = 0;
  memcpy(w, &tmp, sizeof(struct W));
}
