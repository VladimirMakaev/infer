/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstdlib>
#include <vector>

namespace vector_insert_ownership {

struct Box {
  char* p;
};

// Handing an owner to a vector that outlives the call is not a leak. `push_back`
// already got this right; the insertions did not.
void insert_ok(std::vector<Box*>& v) { v.insert(v.begin(), (Box*)malloc(16)); }

void insert_at_end_ok(std::vector<Box*>& v) {
  v.insert(v.end(), (Box*)malloc(16));
}

void insert_count_ok(std::vector<Box*>& v) {
  v.insert(v.begin(), 1, (Box*)malloc(16));
}

void emplace_ok(std::vector<Box*>& v) { v.emplace(v.begin(), (Box*)malloc(16)); }

// Controls on the paths that already worked.
void push_back_ok(std::vector<Box*>& v) { v.push_back((Box*)malloc(16)); }

void emplace_back_ok(std::vector<Box*>& v) {
  v.emplace_back((Box*)malloc(16));
}

// KNOWN LIMITATION: the initializer-list overload takes no single value argument,
// so there is nothing to tie. Recorded rather than hidden.
void FP_insert_initializer_list_ok(std::vector<Box*>& v) {
  v.insert(v.begin(), {(Box*)malloc(16)});
}

// TRUE POSITIVES: the vector is local, so what it owns dies with it. These keep
// reporting because the vector's destructor invalidates its elements.
void local_insert_bad() {
  std::vector<Box*> v;
  v.insert(v.begin(), (Box*)malloc(16));
}

void local_insert_count_bad() {
  std::vector<Box*> v;
  v.insert(v.begin(), 1, (Box*)malloc(16));
}

void local_emplace_bad() {
  std::vector<Box*> v;
  v.emplace(v.begin(), (Box*)malloc(16));
}

void local_push_back_bad() {
  std::vector<Box*> v;
  v.push_back((Box*)malloc(16));
}

struct Owner {
  std::vector<Box*> v;
};

void local_member_vector_bad() {
  Owner o;
  o.v.insert(o.v.begin(), (Box*)malloc(16));
}

// The inserted value is still reachable, so using it after the insert is fine.
void insert_then_use_ok(std::vector<Box*>& v) {
  Box* b = (Box*)malloc(16);
  if (b) {
    v.insert(v.begin(), b);
    b->p = nullptr;
  }
}

void plain_leak_bad() { malloc(16); }

} // namespace vector_insert_ownership
