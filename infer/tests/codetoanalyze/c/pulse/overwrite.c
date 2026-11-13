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

struct hdr {
  int type;
  int len;
};

struct msg {
  struct hdr h;
  struct ops* ops;
};

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

int memset_memcpy_unknown_size_ok(const void* src, size_t n) {
  struct dev d;
  memset(&d, 0, sizeof(struct dev));
  memcpy(&d, src, n);
  return d.ops->v;
}

void copy_dev(struct dev* dst, const struct dev* src) {
  memmove(dst, src, sizeof(*dst));
}

int memset_memmove_in_callee_ok(const struct dev* src) {
  struct dev d;
  memset(&d, 0, sizeof(struct dev));
  copy_dev(&d, src);
  return d.ops->v;
}

int read_dev(int fd, struct dev* d) {
  return read(fd, d, sizeof(*d)) == sizeof(*d);
}

int memset_read_in_callee_ok(int fd) {
  struct dev d;
  memset(&d, 0, sizeof(struct dev));
  if (!read_dev(fd, &d)) {
    return -1;
  }
  return d.ops->v;
}

int fread_dev(FILE* f, struct dev* d) {
  return fread(d, 1, sizeof(*d), f) == sizeof(*d);
}

int memset_fread_in_callee_ok(FILE* f) {
  struct dev d;
  memset(&d, 0, sizeof(struct dev));
  if (!fread_dev(f, &d)) {
    return -1;
  }
  return d.ops->v;
}

void copy_bytes(void* dst, const void* src, size_t n) { memcpy(dst, src, n); }

int memset_copy_bytes_in_callee_ok(const struct dev* src) {
  struct dev d;
  memset(&d, 0, sizeof(struct dev));
  copy_bytes(&d, src, sizeof(struct dev));
  return d.ops->v;
}

void copy_bytes_in_callee_leak_bad(const struct dev* src) {
  struct dev* d = malloc(sizeof(struct dev));
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

int memset_read_bytes_in_callee_ok(int fd) {
  struct dev d;
  memset(&d, 0, sizeof(struct dev));
  if (!read_bytes(fd, &d, sizeof(struct dev))) {
    return -1;
  }
  return d.ops->v;
}

void memset_read_bytes_in_callee_then_check_bad(int fd) {
  struct dev d;
  memset(&d, 0, sizeof(struct dev));
  if (!read_bytes(fd, &d, sizeof(struct dev)) || d.id != 42) {
    return;
  }
  int* p = NULL;
  *p = 42;
}

int copy_bytes_then_read_ops(struct dev* d, const void* src, size_t n) {
  memcpy(d, src, n);
  return d->ops->v;
}

int memset_copy_bytes_then_read_in_callee_ok(const struct dev* src) {
  struct dev d;
  memset(&d, 0, sizeof(struct dev));
  return copy_bytes_then_read_ops(&d, src, sizeof(struct dev));
}

int memset_copy_header_null_field_bad(const struct hdr* h) {
  struct msg m;
  memset(&m, 0, sizeof(struct msg));
  memcpy(&m, h, sizeof(struct hdr));
  return m.ops->v;
}

void memset_read_header_bad(int fd) {
  struct msg m;
  memset(&m, 0, sizeof(struct msg));
  if (read(fd, &m, sizeof(struct hdr)) <= 0) {
    return;
  }
  if (m.h.type == 1) {
    int* p = NULL;
    *p = 42;
  }
}

ssize_t read_first_field_aliases_object_ok(int fd, struct msg* m) {
  if ((void*)&m->h != (void*)m) {
    return -1;
  }
  return read(fd, m, sizeof(struct msg));
}

int stat_file(const char* path, struct stat* st) { return stat(path, st); }

void memset_stat_in_callee_bad(const char* path) {
  struct stat st;
  memset(&st, 0, sizeof(struct stat));
  if (stat_file(path, &st) == 0 && st.st_size != 0) {
    int* p = NULL;
    *p = 42;
  }
}

int memset_init_through_function_pointer_ok(void (*init)(struct dev*)) {
  struct dev d;
  memset(&d, 0, sizeof(struct dev));
  init(&d);
  return d.ops->v;
}

struct dev_ops {
  void (*init)(struct dev*);
};

int memset_init_through_ops_table_ok(const struct dev_ops* dev_ops) {
  struct dev d;
  memset(&d, 0, sizeof(struct dev));
  dev_ops->init(&d);
  return d.ops->v;
}

void (*global_init)(struct dev*);

int memset_init_through_global_callback_ok() {
  struct dev d;
  memset(&d, 0, sizeof(struct dev));
  global_init(&d);
  return d.ops->v;
}

int memset_function_pointer_const_arg_bad(void (*use)(const struct dev*)) {
  struct dev d;
  memset(&d, 0, sizeof(struct dev));
  const struct dev* p = &d;
  use(p);
  return d.ops->v;
}

int memset_function_pointer_converted_const_arg_bad(
    void (*use)(const struct dev*)) {
  struct dev d;
  memset(&d, 0, sizeof(struct dev));
  use(&d);
  return d.ops->v;
}

typedef void (*dev_use_fn)(const struct dev*);

struct dev_users {
  dev_use_fn use;
};

int memset_ops_table_converted_const_arg_bad(const struct dev_users* users) {
  struct dev d;
  memset(&d, 0, sizeof(struct dev));
  users->use(&d);
  return d.ops->v;
}

struct frame {
  char tag[8];
  struct ops* ops;
};

// copying an array that is the first field overwrites the whole object
int FN_memset_copy_first_array_field_null_field_bad(const char* tag) {
  struct frame f;
  memset(&f, 0, sizeof(struct frame));
  memcpy(&f, tag, sizeof(f.tag));
  return f.ops->v;
}

struct cells8 {
  long a, b, c, d, e, f, g, h;
};

struct big {
  struct ops* ops;
  struct cells8 c1, c2, c3, c4, c5, c6, c7, c8;
};

int memset_memcpy_big_struct_ok(const struct big* src) {
  struct big b;
  memset(&b, 0, sizeof(struct big));
  memcpy(&b, src, sizeof(struct big));
  return b.ops->v;
}

// objects with more than 64 scalar and pointer cells are not copied cell by
// cell
int FN_memcpy_big_struct_copies_null_bad(const struct big* src) {
  struct big b;
  memcpy(&b, src, sizeof(struct big));
  b.ops = NULL;
  struct big c;
  memcpy(&c, &b, sizeof(struct big));
  return c.ops->v;
}

// A wholesale write whose destination type is KNOWN covers exactly one whole object, so a
// pointer the object held is really gone and the memory it owned is leaked. Objects small
// enough to have their cells written one by one already reported this; these have more
// scalar cells than the model writes individually, so memcpy takes the coarse path, which
// used to drop the allocation along with the pointer.
struct big_holder {
  void* p;
  int pad0;
  int pad1;
  int pad2;
  int pad3;
  int pad4;
  int pad5;
  int pad6;
  int pad7;
  int pad8;
  int pad9;
  int pad10;
  int pad11;
  int pad12;
  int pad13;
  int pad14;
  int pad15;
  int pad16;
  int pad17;
  int pad18;
  int pad19;
  int pad20;
  int pad21;
  int pad22;
  int pad23;
  int pad24;
  int pad25;
  int pad26;
  int pad27;
  int pad28;
  int pad29;
  int pad30;
  int pad31;
  int pad32;
  int pad33;
  int pad34;
  int pad35;
  int pad36;
  int pad37;
  int pad38;
  int pad39;
  int pad40;
  int pad41;
  int pad42;
  int pad43;
  int pad44;
  int pad45;
  int pad46;
  int pad47;
  int pad48;
  int pad49;
  int pad50;
  int pad51;
  int pad52;
  int pad53;
  int pad54;
  int pad55;
  int pad56;
  int pad57;
  int pad58;
  int pad59;
  int pad60;
  int pad61;
  int pad62;
  int pad63;
  int pad64;
  int pad65;
  int pad66;
  int pad67;
  int pad68;
  int pad69;
  int pad70;
  int pad71;
  int pad72;
  int pad73;
  int pad74;
  int pad75;
  int pad76;
  int pad77;
  int pad78;
  int pad79;
};

void big_memcpy_leaks_bad(void) {
  struct big_holder h;
  h.p = malloc(8);
  struct big_holder zero;
  memset(&zero, 0, sizeof(zero));
  memcpy(&h, &zero, sizeof(h));
}

// Additional coverage, NOT a discriminator: memset goes through fill_object, which writes
// the pointer cells of the known type directly and never took the coarse path, so this
// already reported. It is here so that a future change to that path is noticed.
void big_memset_leaks_bad(void) {
  struct big_holder h;
  h.p = malloc(8);
  memset(&h, 0, sizeof(h));
}

// the write covers the object but the memory was already handed back: nothing to report
void big_memset_after_free_ok(void) {
  struct big_holder h;
  h.p = malloc(8);
  free(h.p);
  memset(&h, 0, sizeof(h));
}

// the pointer survives the write, so there is nothing to report either
void big_memset_pointer_kept_ok(void) {
  struct big_holder h;
  h.p = malloc(8);
  void* keep = h.p;
  memset(&h, 0, sizeof(h));
  free(keep);
}

void fill_through_unknown_callee(void* dest, size_t n);

// FN: an unknown callee may write any part of the object, so the pointer it held may well
// survive; keeping the allocation here would report leaks that are not leaks. The extent is
// the missing information, not the type.
void FN_unknown_callee_leaks_bad(void) {
  struct big_holder h;
  h.p = malloc(8);
  fill_through_unknown_callee(&h, sizeof(h));
}

// The two cases above that are silent go through memset, which fills the cells of the known
// type directly and never took the coarse path. These two are the same controls ON the coarse
// path, so they are the ones that constrain it.
void big_memcpy_pointer_kept_ok(void) {
  struct big_holder h;
  h.p = malloc(8);
  void* keep = h.p;
  struct big_holder zero;
  memset(&zero, 0, sizeof(zero));
  memcpy(&h, &zero, sizeof(h));
  free(keep);
}

void big_memcpy_after_free_ok(void) {
  struct big_holder h;
  h.p = malloc(8);
  free(h.p);
  struct big_holder zero;
  memset(&zero, 0, sizeof(zero));
  memcpy(&h, &zero, sizeof(h));
}

// Copying the object OUT takes the same coarse path, which gives the destination unknown
// contents, so nothing would record that the copy holds the pointer. The copy says so instead,
// and the write over the original then gives the memory up rather than calling it leaked.
void big_memcpy_saved_copy_ok(void) {
  struct big_holder h;
  h.p = malloc(8);
  struct big_holder backup;
  memcpy(&backup, &h, sizeof(h));
  struct big_holder zero;
  memset(&zero, 0, sizeof(zero));
  memcpy(&h, &zero, sizeof(h));
  free(backup.p);
}

// The same copy with no write over the original afterwards: the object still holds the only
// pointer when it goes out of scope, so this is a leak and saying the copy exists must not
// hide it. This is what dropping the allocation at the copy instead would have silenced.
void big_memcpy_copied_out_then_leaked_bad(void) {
  struct big_holder h;
  h.p = malloc(8);
  struct big_holder backup;
  memcpy(&backup, &h, sizeof(h));
}

static void copy_big_holder(struct big_holder* dest, const struct big_holder* src) {
  memcpy(dest, src, sizeof(*dest));
}

// The copy happens in a callee, which is how it is usually written. The callee never reads the
// source's fields, so its summary has no cells there to speak about; what it carries is the
// source's ADDRESS, which the caller knows as `h`.
void big_memcpy_saved_by_callee_ok(void) {
  struct big_holder h;
  h.p = malloc(8);
  struct big_holder backup;
  copy_big_holder(&backup, &h);
  struct big_holder zero;
  memset(&zero, 0, sizeof(zero));
  memcpy(&h, &zero, sizeof(h));
  free(backup.p);
}

struct holder_wrapper {
  struct big_holder inner;
};

// A copy of the whole object copied every part of it, so a write over one part may not claim
// that part's memory either -- even though the write starts below where the copy was made.
void big_field_overwrite_after_whole_copy_ok(void) {
  struct holder_wrapper w;
  w.inner.p = malloc(8);
  struct holder_wrapper backup;
  memcpy(&backup, &w, sizeof(w));
  struct big_holder zero;
  memset(&zero, 0, sizeof(zero));
  memcpy(&w.inner, &zero, sizeof(w.inner));
  free(backup.inner.p);
}

// and the control for it: no copy was made, so the same write does report
void big_field_overwrite_no_copy_bad(void) {
  struct holder_wrapper w;
  w.inner.p = malloc(8);
  struct big_holder zero;
  memset(&zero, 0, sizeof(zero));
  memcpy(&w.inner, &zero, sizeof(w.inner));
}

// Only the object that was copied is spoken for: `g` was not, so overwriting it still reports.
void big_other_object_still_leaks_bad(void) {
  struct big_holder h;
  h.p = malloc(8);
  struct big_holder g;
  g.p = malloc(8);
  struct big_holder backup;
  memcpy(&backup, &h, sizeof(h));
  struct big_holder zero;
  memset(&zero, 0, sizeof(zero));
  memcpy(&g, &zero, sizeof(g));
  free(h.p);
  free(backup.p);
}

// FN: the copy is cleared before the original is, so by the end nobody holds the pointer and
// it is leaked. Saying an object was copied away is never taken back, so the write over the
// original still gives the memory up. Losing a leak is the safe direction here.
void FN_big_copy_cleared_first_bad(void) {
  struct big_holder h;
  h.p = malloc(8);
  struct big_holder backup;
  memcpy(&backup, &h, sizeof(h));
  struct big_holder zero;
  memset(&zero, 0, sizeof(zero));
  memcpy(&backup, &zero, sizeof(backup));
  memcpy(&h, &zero, sizeof(h));
}

// Asking for the size of the object is not writing all of it: both of these may transfer
// fewer bytes and leave the pointer where it was, so neither may claim the memory is gone.
void big_read_short_ok(int fd) {
  struct big_holder h;
  h.p = malloc(8);
  read(fd, &h, sizeof(h));
}

void big_fread_short_ok(FILE* f) {
  struct big_holder h;
  h.p = malloc(8);
  fread(&h, sizeof(h), 1, f);
}

struct wide_holder {
  struct big_holder head;
  void* tail;
};

// Writing a whole object says nothing about the object it was written INTO. The copy covers
// head, so tail still holds what it held and is freed through it.
void wide_tail_outside_the_copy_ok(void) {
  struct wide_holder w;
  w.head.p = 0;
  w.tail = malloc(8);
  struct big_holder z;
  memset(&z, 0, sizeof(z));
  memcpy(&w, &z, sizeof(z));
  free(w.tail);
}

// the twin that makes the silence above readable: the same object and the same copy, aimed at
// what the copied type really covers, and the pointer there is gone
void wide_head_inside_the_copy_bad(void) {
  struct wide_holder w;
  w.head.p = malloc(8);
  w.tail = 0;
  struct big_holder z;
  memset(&z, 0, sizeof(z));
  memcpy(&w.head, &z, sizeof(z));
}

// FN: the tail is leaked here, and it stays unreported for the same reason the case above
// stays silent. The copy gives the whole destination unknown contents, including the bytes it
// did not write, so after it nothing tells the two apart -- keeping the tail's memory to report
// this would report the case above, where it is freed. Both are silent without the copy too.
void FN_wide_tail_outside_the_copy_leaks_bad(void) {
  struct wide_holder w;
  w.head.p = 0;
  w.tail = malloc(8);
  struct big_holder z;
  memset(&z, 0, sizeof(z));
  memcpy(&w, &z, sizeof(z));
}

// An element is not the array: the copy covers the first one, so the second still owns its
// memory. The destination decays to the element type here, which is why the copied type has to
// be measured against the object written into rather than compared with it.
void array_element_sibling_ok(void) {
  struct big_holder arr[2];
  arr[0].p = 0;
  arr[1].p = malloc(8);
  struct big_holder z;
  memset(&z, 0, sizeof(z));
  memcpy(arr, &z, sizeof(z));
  free(arr[1].p);
}

// the twin for it: the copy is aimed at the element that holds the memory, which is gone
void array_element_inside_the_copy_bad(void) {
  struct big_holder arr[2];
  arr[0].p = 0;
  arr[1].p = malloc(8);
  struct big_holder z;
  memset(&z, 0, sizeof(z));
  memcpy(&arr[1], &z, sizeof(z));
}
