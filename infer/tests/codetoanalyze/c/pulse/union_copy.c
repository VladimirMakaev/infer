/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

struct payload {
  int value;
};

union data {
  struct components {
    int code;
    int salt;
  } components;
  unsigned word;
};

// C has no assignment operator: the whole object is copied in one store, and reading a
// member of the destination must see what the source held.
int union_assigned_ok() {
  union data a = {.components = {.code = 0, .salt = 0}};
  union data b = {.components = {.code = 1, .salt = 0}};
  b = a;
  if (b.components.code != 0) {
    struct payload* p = 0;
    return p->value;
  }
  return 0;
}

// Control: the same whole-object assignment of a struct was never affected.
struct plain {
  struct components components;
  unsigned word;
};

int struct_assigned_ok() {
  struct plain a = {.components = {.code = 0, .salt = 0}};
  struct plain b = {.components = {.code = 1, .salt = 0}};
  b = a;
  if (b.components.code != 0) {
    struct payload* p = 0;
    return p->value;
  }
  return 0;
}

// TRUE POSITIVE: the copy does not make an unchecked dereference safe.
int union_assigned_then_unchecked_bad() {
  union data a = {.components = {.code = 1, .salt = 0}};
  union data b = {.components = {.code = 0, .salt = 0}};
  b = a;
  struct payload* p = 0;
  return p->value;
}
