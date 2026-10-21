/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

namespace subobject_address {

struct Empty {};
struct Plain {
  int n;
};
struct Holder {
  Empty e;
  Plain p;
};
struct Pointer {
  int* q;
};

int field_address_good() {
  Plain p;
  if (&p.n == nullptr) {
    return *static_cast<int*>(nullptr);
  }
  return 0;
}

int empty_field_address_good() {
  Holder h{};
  if (&h.e == nullptr) {
    return *static_cast<int*>(nullptr);
  }
  return 0;
}

int nested_field_address_good() {
  Holder h;
  if (&h.p.n == nullptr) {
    return *static_cast<int*>(nullptr);
  }
  return 0;
}

int array_element_address_good() {
  int a[4];
  if (&a[2] == nullptr) {
    return *static_cast<int*>(nullptr);
  }
  return 0;
}

// the contents of a field, unlike its address, can still be null
int field_contents_bad() {
  Pointer s;
  s.q = nullptr;
  return *s.q;
}

// an unknown pointer is no base to stand on, so this branch is still explored
int FP_unknown_base_field_good(Plain* p) {
  if (&p->n == nullptr) {
    return *static_cast<int*>(nullptr);
  }
  return 0;
}

int direct_null_bad() { return *static_cast<int*>(nullptr); }

} // namespace subobject_address
