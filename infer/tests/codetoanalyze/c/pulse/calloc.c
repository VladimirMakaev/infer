/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct ops {
  int v;
};

struct dev {
  int id;
  struct ops* ops;
};

struct outer {
  int x;
  struct dev inner;
};

void unknown_init(struct dev* d);

int calloc_null_field_bad() {
  struct dev* d = calloc(1, sizeof(struct dev));
  if (!d) {
    return -1;
  }
  int r = d->ops->v;
  free(d);
  return r;
}

int calloc_swapped_args_null_field_bad() {
  struct dev* d = calloc(sizeof(*d), 1);
  if (!d) {
    return -1;
  }
  int r = d->ops->v;
  free(d);
  return r;
}

int calloc_nested_null_field_bad() {
  struct outer* o = calloc(1, sizeof(struct outer));
  if (!o) {
    return -1;
  }
  int r = o->inner.ops->v;
  free(o);
  return r;
}

int calloc_pointer_null_bad() {
  int** p = calloc(1, sizeof(int*));
  if (!p) {
    return -1;
  }
  int r = **p;
  free(p);
  return r;
}

void calloc_int_field_zero_ok() {
  struct dev* d = calloc(1, sizeof(struct dev));
  if (!d) {
    return;
  }
  if (d->id != 0) {
    int* p = NULL;
    *p = 42;
  }
  free(d);
}

struct dev* make_dev() { return calloc(1, sizeof(struct dev)); }

int calloc_in_callee_null_field_bad() {
  struct dev* d = make_dev();
  if (!d) {
    return -1;
  }
  int r = d->ops->v;
  free(d);
  return r;
}

int calloc_set_field_ok(struct ops* o) {
  struct dev* d = calloc(1, sizeof(struct dev));
  if (!d) {
    return -1;
  }
  d->ops = o;
  int r = d->ops->v;
  free(d);
  return r;
}

int calloc_unknown_init_ok() {
  struct dev* d = calloc(1, sizeof(struct dev));
  if (!d) {
    return -1;
  }
  unknown_init(d);
  int r = d->ops->v;
  free(d);
  return r;
}

int calloc_memcpy_ok(const struct dev* src) {
  struct dev* d = calloc(1, sizeof(struct dev));
  if (!d) {
    return -1;
  }
  memcpy(d, src, sizeof(struct dev));
  int r = d->ops->v;
  free(d);
  return r;
}

void calloc_sscanf_field_bad(const char* s) {
  struct dev* d = calloc(1, sizeof(struct dev));
  if (!d) {
    return;
  }
  if (sscanf(s, "%d", &d->id) == 1 && d->id != 0) {
    int* p = NULL;
    *p = 42;
  }
  free(d);
}

// arrays are not zero-filled since Pulse does not relate d[0].ops to d->ops
int calloc_array_index_write_ok(int n, struct ops* o) {
  struct dev* d = calloc(n, sizeof(struct dev));
  if (!d) {
    return -1;
  }
  d[0].ops = o;
  int r = d->ops->v;
  free(d);
  return r;
}

// arrays are not zero-filled
int FN_calloc_array_null_field_bad(int n) {
  struct dev* d = calloc(n, sizeof(struct dev));
  if (!d) {
    return -1;
  }
  int r = d->ops->v;
  free(d);
  return r;
}

// Pulse does not relate d[0].ops to d->ops, so d->ops still reads the zero
// written by calloc
int FP_calloc_index_write_arrow_read_ok(struct ops* o) {
  struct dev* d = calloc(1, sizeof(struct dev));
  if (!d) {
    return -1;
  }
  d[0].ops = o;
  int r = d->ops->v;
  free(d);
  return r;
}

struct link {
  struct ops* ops;
  int len;
};

struct node {
  struct link l;
  int n;
};

void fill_link(struct node* n, const struct link* src) {
  memcpy(n, src, sizeof(struct link));
}

int calloc_copy_header_in_callee_ok(const struct link* src) {
  struct node* n = calloc(1, sizeof(struct node));
  if (!n) {
    return -1;
  }
  fill_link(n, src);
  int r = n->l.ops->v;
  free(n);
  return r;
}

struct dev make_dev_value(struct ops* o) {
  struct dev d = {1, o};
  return d;
}

// assigning a struct returned by value stores the address of the returned
// temporary instead of copying its fields, so the zeros written by calloc
// survive
int FP_calloc_assign_struct_returned_by_value_ok(struct ops* o) {
  struct dev* d = calloc(1, sizeof(struct dev));
  if (!d) {
    return -1;
  }
  *d = make_dev_value(o);
  int r = d->ops->v;
  free(d);
  return r;
}

struct base {
  struct ops* ops;
};

struct derived {
  struct base b;
  int n;
};

int calloc_upcast_write_ok(struct ops* o) {
  struct derived* d = calloc(1, sizeof(struct derived));
  if (!d) {
    return -1;
  }
  ((struct base*)d)->ops = o;
  int r = d->b.ops->v;
  free(d);
  return r;
}

void init_base(struct base* b, struct ops* o) { b->ops = o; }

int calloc_upcast_init_in_callee_ok(struct ops* o) {
  struct derived* d = calloc(1, sizeof(struct derived));
  if (!d) {
    return -1;
  }
  init_base((struct base*)d, o);
  int r = d->b.ops->v;
  free(d);
  return r;
}

struct tagged {
  int kind;
  union {
    struct ops* ops;
    void* data;
  } u;
};

int calloc_union_write_other_member_ok(void* data) {
  struct tagged* t = calloc(1, sizeof(struct tagged));
  if (!t) {
    return -1;
  }
  t->u.data = data;
  int r = t->u.ops->v;
  free(t);
  return r;
}

// Pulse does not relate the cell written through a char pointer to d->ops
int FP_calloc_write_through_char_pointer_ok(struct ops* o) {
  struct dev* d = calloc(1, sizeof(struct dev));
  if (!d) {
    return -1;
  }
  *(struct ops**)((char*)d + offsetof(struct dev, ops)) = o;
  int r = d->ops->v;
  free(d);
  return r;
}

struct cells8 {
  long a, b, c, d, e, f, g, h;
};

struct big {
  struct ops* ops;
  struct cells8 c1, c2, c3, c4, c5, c6, c7, c8;
};

// objects with more than 64 scalar and pointer cells are not zeroed
int FN_calloc_big_struct_null_field_bad() {
  struct big* b = calloc(1, sizeof(struct big));
  if (!b) {
    return -1;
  }
  int r = b->ops->v;
  free(b);
  return r;
}
