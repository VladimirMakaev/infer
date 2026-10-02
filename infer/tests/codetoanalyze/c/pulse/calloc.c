/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

void explicit_bzero(void* s, size_t n);

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

struct hdr {
  int type;
  int len;
};

struct msg {
  struct hdr h;
  struct ops* ops;
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

int calloc_memcpy_unknown_size_ok(const void* src, size_t n) {
  struct dev* d = calloc(1, sizeof(struct dev));
  if (!d) {
    return -1;
  }
  memcpy(d, src, n);
  int r = d->ops->v;
  free(d);
  return r;
}

void copy_dev(struct dev* dst, const struct dev* src) {
  memmove(dst, src, sizeof(*dst));
}

int calloc_memmove_in_callee_ok(const struct dev* src) {
  struct dev* d = calloc(1, sizeof(struct dev));
  if (!d) {
    return -1;
  }
  copy_dev(d, src);
  int r = d->ops->v;
  free(d);
  return r;
}

int read_dev(int fd, struct dev* d) {
  return read(fd, d, sizeof(*d)) == sizeof(*d);
}

int calloc_read_in_callee_ok(int fd) {
  struct dev* d = calloc(1, sizeof(struct dev));
  if (!d) {
    return -1;
  }
  if (!read_dev(fd, d)) {
    free(d);
    return -1;
  }
  int r = d->ops->v;
  free(d);
  return r;
}

int fread_dev(FILE* f, struct dev* d) {
  return fread(d, 1, sizeof(*d), f) == sizeof(*d);
}

int calloc_fread_in_callee_ok(FILE* f) {
  struct dev* d = calloc(1, sizeof(struct dev));
  if (!d) {
    return -1;
  }
  if (!fread_dev(f, d)) {
    free(d);
    return -1;
  }
  int r = d->ops->v;
  free(d);
  return r;
}

void copy_bytes(void* dst, const void* src, size_t n) { memcpy(dst, src, n); }

int calloc_copy_bytes_in_callee_ok(const struct dev* src) {
  struct dev* d = calloc(1, sizeof(struct dev));
  if (!d) {
    return -1;
  }
  copy_bytes(d, src, sizeof(struct dev));
  int r = d->ops->v;
  free(d);
  return r;
}

void calloc_copy_bytes_in_callee_leak_bad(const struct dev* src) {
  struct dev* d = calloc(1, sizeof(struct dev));
  if (!d) {
    return;
  }
  copy_bytes(d, src, sizeof(struct dev));
}

int read_bytes(int fd, void* buf, size_t len) {
  char* p = buf;
  while (len > 0) {
    ssize_t n = read(fd, p, len);
    if (n <= 0) {
      return 0;
    }
    p += n;
    len -= n;
  }
  return 1;
}

int calloc_read_bytes_in_callee_ok(int fd) {
  struct dev* d = calloc(1, sizeof(struct dev));
  if (!d) {
    return -1;
  }
  if (!read_bytes(fd, d, sizeof(struct dev))) {
    free(d);
    return -1;
  }
  int r = d->ops->v;
  free(d);
  return r;
}

void calloc_read_bytes_in_callee_then_check_bad(int fd) {
  struct dev* d = calloc(1, sizeof(struct dev));
  if (!d) {
    return;
  }
  if (!read_bytes(fd, d, sizeof(struct dev)) || d->id != 42) {
    free(d);
    return;
  }
  free(d);
  int* p = NULL;
  *p = 42;
}

int copy_bytes_then_read_ops(struct dev* d, const void* src, size_t n) {
  memcpy(d, src, n);
  return d->ops->v;
}

int calloc_copy_bytes_then_read_in_callee_ok(const struct dev* src) {
  struct dev* d = calloc(1, sizeof(struct dev));
  if (!d) {
    return -1;
  }
  int r = copy_bytes_then_read_ops(d, src, sizeof(struct dev));
  free(d);
  return r;
}

int calloc_copy_header_null_field_bad(const struct hdr* h) {
  struct msg* m = calloc(1, sizeof(struct msg));
  if (!m) {
    return -1;
  }
  memcpy(m, h, sizeof(struct hdr));
  int r = m->ops->v;
  free(m);
  return r;
}

void calloc_read_header_bad(int fd) {
  struct msg* m = calloc(1, sizeof(struct msg));
  if (!m) {
    return;
  }
  if (read(fd, m, sizeof(struct hdr)) <= 0) {
    free(m);
    return;
  }
  if (m->h.type == 1) {
    int* p = NULL;
    *p = 42;
  }
  free(m);
}

ssize_t read_first_field_aliases_object_ok(int fd, struct msg* m) {
  if ((void*)&m->h != (void*)m) {
    return -1;
  }
  return read(fd, m, sizeof(struct msg));
}

int stat_file(const char* path, struct stat* st) { return stat(path, st); }

void calloc_stat_in_callee_bad(const char* path) {
  struct stat* st = calloc(1, sizeof(struct stat));
  if (!st) {
    return;
  }
  if (stat_file(path, st) == 0 && st->st_size != 0) {
    int* p = NULL;
    *p = 42;
  }
  free(st);
}

int bzero_null_field_bad() {
  struct dev d;
  bzero(&d, sizeof(struct dev));
  return d.ops->v;
}

int explicit_bzero_null_field_bad() {
  struct dev d;
  explicit_bzero(&d, sizeof(struct dev));
  return d.ops->v;
}

int builtin_memset_null_field_bad() {
  struct dev d;
  __builtin_memset(&d, 0, sizeof(struct dev));
  return d.ops->v;
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

// the callee writes the cells of a struct link at the address of n, not the
// ones of n->l, so n->l.ops still reads the zero written by calloc
int FP_calloc_copy_header_in_callee_ok(const struct link* src) {
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

// unresolved calls through function pointers do not havoc their arguments, so
// the zero written by calloc survives the call
int FP_calloc_init_through_function_pointer_ok(void (*init)(struct dev*)) {
  struct dev* d = calloc(1, sizeof(struct dev));
  if (!d) {
    return -1;
  }
  init(d);
  int r = d->ops->v;
  free(d);
  return r;
}
