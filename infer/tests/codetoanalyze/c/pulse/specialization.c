/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <stdlib.h>

int invoke(int (*f)(int), int i) { return (*f)(i); }

int id(int i) { return i; }

int add_one(int i) { return invoke(id, i) + 1; }

int add_two(int i) {
  int one = invoke(id, 1);
  add_one(0);
  return invoke(add_one, i) + 1;
}

// recursion cycle
int add_more_bad(int i) {
  if (i > 0) {
    return invoke(add_more_bad, i - 1) + 1;
  }
  return 0;
}

void test_invoke_ok() {
  if (add_one(0) != 1) {
    int* p = NULL;
    *p = 42;
  }
}

void test_invoke_bad() {
  if (add_one(0) == 1) {
    int* p = NULL;
    *p = 42;
  }
}

void test_recursive_invoke_ok() {
  if (add_two(0) + add_one(0) != 3) {
    int* p = NULL;
    *p = 42;
  }
}

void test_recursive_invoke_bad() {
  if (add_two(0) + add_one(0) == 3) {
    int* p = NULL;
    *p = 42;
  }
}

// recursion cycle involving alias specialization
int two_pointers_recursion_bad(int* x, int* y, int i) {
  if (i > 0) {
    return *x + *y + two_pointers_recursion_bad(x, x, i - 1);
  }
  return 0;
}

void alias_recursion(int* z) { two_pointers_recursion_bad(z, z, 10); }

// recursion cycle involving closure specialization
void invoke_itself_bad(int (*f)(int), int i) {
  if (i > 0) {
    (*f)(i);
    invoke_itself_bad(f, i - 1);
  }
}

void specialize_invoke_itself_ok() { invoke_itself_bad(id, 10); }

void may_double_free_if_alias(int* x, int* y) {
  int a = *x;
  int b = *y;

  free(x);
  free(y);
}

void call_may_double_free_if_alias_bad() {
  int* z = (int*)malloc(sizeof(int));
  if (z == NULL)
    return 0;
  *z = 1;

  may_double_free_if_alias(z, z);
}

int test_alias(int* p, int* q) {
  *q = 2;
  *p = 1;
  return (*p == *q);
}

void call_test_alias_bad(int* x) {
  if (test_alias(x, x)) {
    int* p = NULL;
    *p = 42;
  };
}

void call_test_alias_good(int* x) {
  if (!test_alias(x, x)) {
    int* p = NULL;
    *p = 42;
  };
}

int test_unalias(int* p, int* q) {
  *q = 2;
  *p = 1;
  return (*p != *q);
}

void call_test_unalias_bad(int* x) {
  if (!test_unalias(x, x)) {
    int* p = NULL;
    *p = 42;
  };
}

void call_test_unalias_good(int* x) {
  if (test_unalias(x, x)) {
    int* p = NULL;
    *p = 42;
  };
}

int* global_p;
int* global_q;

int test_global_alias() {
  *global_q = 2;
  *global_p = 1;
  return (*global_p == *global_q);
}

void call_test_global_alias_bad(int* x) {
  global_p = x;
  global_q = x;
  if (test_global_alias()) {
    int* p = NULL;
    *p = 42;
  };
}

void call_test_global_alias_good(int* x) {
  global_p = x;
  global_q = x;
  if (!test_global_alias()) {
    int* p = NULL;
    *p = 42;
  };
}

// Aliased formals where the freed one is never accessed in the callee.
//
// `may_double_free_if_alias` above dereferences BOTH formals before freeing. These
// cases differ only in which formals the callee touches: `free` alone does not put its
// argument in the callee's heap, so the alias test has to recognise an invalidated
// value as one whose identity matters. Every `_bad` here faults at run time.

int free_first_formal_read_second(int* a, int* b) {
  free(a);
  return *b;
}

void call_free_first_formal_read_second_bad(void) {
  int* p = (int*)malloc(sizeof(int));
  if (p == NULL) {
    return;
  }
  *p = 1;
  free_first_formal_read_second(p, p);
}

// Same, reached through a second name for the pointer rather than the same
// expression twice: the caller's aliasing must not depend on how it is spelled.
void call_free_first_formal_read_second_via_copy_bad(void) {
  int* p = (int*)malloc(sizeof(int));
  if (p == NULL) {
    return;
  }
  *p = 1;
  int* q = p;
  free_first_formal_read_second(p, q);
}

// The other formal is read BEFORE the free, so only the freed one is untouched. This
// is the one-token difference from `read_freed_formal_first` below.
int read_second_formal_then_free_first(int* a, int* b) {
  int t = *b;
  (void)t;
  free(a);
  return *b;
}

void call_read_second_formal_then_free_first_bad(void) {
  int* p = (int*)malloc(sizeof(int));
  if (p == NULL) {
    return;
  }
  *p = 1;
  read_second_formal_then_free_first(p, p);
}

// The access after the free is a write rather than a read.
void free_first_formal_write_second(int* a, int* b) {
  free(a);
  *b = 7;
}

void call_free_first_formal_write_second_bad(void) {
  int* p = (int*)malloc(sizeof(int));
  if (p == NULL) {
    return;
  }
  *p = 1;
  free_first_formal_write_second(p, p);
}

// CONTROL, and the half of the one-token pair that already works: the freed formal is
// dereferenced before it is freed, so it is in the callee's heap and the alias is
// found. A fix must keep this reporting as well as make the four above report -- the
// bug is the DIFFERENCE between this and
// `read_second_formal_then_free_first`, which differ only in whether the dead read
// touches `a` or `b`.
int read_freed_formal_first(int* a, int* b) {
  int t = *a;
  (void)t;
  free(a);
  return *b;
}

void call_read_freed_formal_first_bad(void) {
  int* p = (int*)malloc(sizeof(int));
  if (p == NULL) {
    return;
  }
  *p = 1;
  read_freed_formal_first(p, p);
}

// CONTROL: distinct allocations, so there is no aliasing and nothing to report. If a
// widened alias test started reporting here it would be inventing an alias the caller
// does not have.
void call_free_first_formal_read_second_distinct_ok(void) {
  int* p = (int*)malloc(sizeof(int));
  int* q = (int*)malloc(sizeof(int));
  if (p == NULL || q == NULL) {
    free(p);
    free(q);
    return;
  }
  *p = 1;
  *q = 2;
  free_first_formal_read_second(p, q);
  free(q);
}

// CONTROL: the same callee, aliased formals, but the caller never let the pointer
// become invalid -- it is a local's address, which is not freeable and not freed here.
// Reporting here would mean the widened test fires on aliasing alone rather than on
// aliasing plus invalidation.
int read_both_formals(int* a, int* b) { return *a + *b; }

void call_read_both_formals_aliased_ok(void) {
  int v = 3;
  int* p = &v;
  read_both_formals(p, p);
}
