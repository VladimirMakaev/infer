/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <stdlib.h>
#include <stdarg.h>

int sum(int n, ...) {
  va_list args;
  va_start(args, n);
  int sum = 0;
  for (int i = 0; i < n; i++) {
    sum += va_arg(args, int);
  }
  va_end(args);
  return sum;
}

void sum_one_then_npe_bad() {
  int one = sum(1, 1);
  int* p = NULL;
  *p = one;
}

// we run out of loop iterations before reaching 4
void FN_sum_four_then_npe_bad() {
  int four = sum(4, 1, 1, 1, 1);
  int* p = NULL;
  *p = four;
}

// we run out of loop iterations before reaching 4
void FN_sum_then_reachable_npe_bad() {
  int four = sum(4, 1, 1, 1, 1);
  if (four == 4) {
    int* p = NULL;
    *p = 42;
  }
}

// the arguments are resolved, so the sum is known and this branch is dead
void sum_then_unreachable_npe_ok() {
  int one = sum(1, 1);
  if (one == 4) {
    int* p = NULL;
    *p = 42;
  }
}

int unknown_sum(int n, ...);

void unknown_sum_one_then_npe_bad() {
  int one = unknown_sum(1, 1);
  int* p = NULL;
  *p = one;
}

void unknown_sum_four_then_npe_bad() {
  int four = unknown_sum(4, 1, 1, 1, 1);
  int* p = NULL;
  *p = four;
}

static int storage;

void fill_out_param(int n, ...) {
  va_list args;
  va_start(args, n);
  int** p = va_arg(args, int**);
  *p = &storage;
  va_end(args);
}

// the callee writes the caller's pointer through a va_arg pointer
void writes_are_seen_ok() {
  int* v;
  fill_out_param(1, &v);
  int x = *v;
}

void write_null_out_param(int n, ...) {
  va_list args;
  va_start(args, n);
  int** p = va_arg(args, int**);
  *p = NULL;
  va_end(args);
}

// and a write of NULL through one is still a bug in the caller
void writes_null_bad() {
  int* v;
  write_null_out_param(1, &v);
  int x = *v;
}

void free_out_param(int n, ...) {
  va_list args;
  va_start(args, n);
  char** p = va_arg(args, char**);
  free(*p);
  va_end(args);
}

// as is a free through one
void frees_then_uses_bad() {
  char* v = (char*)malloc(8);
  if (v) {
    free_out_param(1, &v);
    *v = 'x';
  }
}

int only_reads(int n, ...) {
  va_list args;
  va_start(args, n);
  int* q = va_arg(args, int*);
  int v = *q;
  va_end(args);
  return v;
}

// a callee that only reads leaves the caller's value alone
void read_only_callee_ok() {
  int x = 5;
  only_reads(1, &x);
  if (x != 5) {
    int* p = NULL;
    *p = 42;
  }
}

/* --- the cases we deliberately do not resolve: each must behave as it did
   before any of this, i.e. no aliasing and no havoc --- */

void fill_in_loop(int n, ...) {
  va_list args;
  va_start(args, n);
  for (int i = 0; i < n; i++) {
    int** p = va_arg(args, int**);
    *p = &storage;
  }
  va_end(args);
}

// a counted loop is unrolled, so each read is at a position we can name
void counted_loop_ok() {
  int *v, *u;
  fill_in_loop(2, &v, &u);
  int x = *u;
}

void fill_until_null(int n, ...) {
  va_list args;
  va_start(args, n);
  int** p;
  while ((p = va_arg(args, int**))) {
    *p = &storage;
  }
  va_end(args);
}

// a short sentinel loop is still unrolled, so these are named too
void sentinel_loop_ok() {
  int *v, *u;
  fill_until_null(0, &v, &u, NULL);
  int x = *u;
}

// FALLBACK: past the iterations that are unrolled the position is no longer
// one we can name, so the later arguments are not resolved
void FN_fallback_deep_loop_ok() {
  int *a, *b, *c, *d, *e, *f;
  fill_until_null(0, &a, &b, &c, &d, &e, &f, NULL);
  int x = *f;
}

void fill_after_copy(int n, ...) {
  va_list args, copy;
  va_start(args, n);
  va_copy(copy, args);
  int** p = va_arg(copy, int**);
  *p = &storage;
  va_end(copy);
  va_end(args);
}

// FALLBACK: a copied list walks the arguments independently of the original
void FN_fallback_va_copy_ok() {
  int* v;
  fill_after_copy(1, &v);
  int x = *v;
}

void hand_off(va_list args);

void fill_via_callee(int n, ...) {
  va_list args;
  va_start(args, n);
  hand_off(args);
  int** p = va_arg(args, int**);
  *p = &storage;
  va_end(args);
}

// FALLBACK: the list went to another function, which may have advanced it
void FN_fallback_handed_off_ok() {
  int* v;
  fill_via_callee(1, &v);
  int x = *v;
}
