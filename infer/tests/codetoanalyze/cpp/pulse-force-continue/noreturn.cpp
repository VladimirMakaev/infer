/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstdlib>
#include <exception>

void die() { std::terminate(); }

void check_not_null(int* p) {
  if (p == nullptr) {
    die();
  }
}

int deref_checked(int* p) {
  check_not_null(p);
  return *p;
}

int deref_checked_null_ok() { return deref_checked(nullptr); }

#define EXPECTS(cond) \
  ((cond) ? static_cast<void>(0) : ([] { std::terminate(); })())

int deref_expects(int* p) {
  EXPECTS(p != nullptr);
  return *p;
}

int deref_expects_null_ok() { return deref_expects(nullptr); }

// more values than the default disjunct limit: some are dropped
int many_values(int x) {
  switch (x) {
    case 0:
      return 0;
    case 1:
      return 1;
    case 2:
      return 2;
    case 3:
      return 3;
    case 4:
      return 4;
    case 5:
      return 5;
    case 6:
      return 6;
    case 7:
      return 7;
    case 8:
      return 8;
    case 9:
      return 9;
    case 10:
      return 10;
    case 11:
      return 11;
    case 12:
      return 12;
    case 13:
      return 13;
    case 14:
      return 14;
    case 15:
      return 15;
    case 16:
      return 16;
    case 17:
      return 17;
    case 18:
      return 18;
    case 19:
      return 19;
    default:
      return 20;
  }
}

// all the disjuncts left exit but the call is treated as unknown since some
// were dropped
void exit_unless_dropped_value(int x) {
  int y = many_values(x);
  if (y != 0) {
    exit(1);
  }
}

void call_exit_unless_dropped_value_bad() {
  exit_unless_dropped_value(0);
  int* p = nullptr;
  *p = 42;
}

void deref_null_bad() {
  int* p = nullptr;
  *p = 42;
}

// every spec of the callee is an error: the call is treated as unknown
void call_deref_null_then_deref_null_bad() {
  deref_null_bad();
  int* q = nullptr;
  *q = 42;
}

void deref_null_if_bad(int x) {
  if (x != 0) {
    int* p = nullptr;
    *p = 42;
  }
}

// the error is the only spec that applies here
void call_deref_null_if_then_deref_null_bad() {
  deref_null_if_bad(1);
  int* q = nullptr;
  *q = 42;
}
