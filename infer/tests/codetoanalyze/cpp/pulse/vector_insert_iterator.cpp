/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstdlib>
#include <vector>

namespace vector_insert_iterator {

void insert_unknown_ok(std::vector<int*>& v) {
  auto it = v.insert(v.begin(), nullptr);
  *it = static_cast<int*>(malloc(sizeof(int)));
}

void insert_spare_end_ok(std::vector<int*>& v) {
  v.clear();
  v.reserve(4);
  v.push_back(nullptr);
  auto it = v.insert(v.end(), nullptr);
  *it = static_cast<int*>(malloc(sizeof(int)));
}

void insert_spare_middle_ok(std::vector<int*>& v) {
  v.clear();
  v.reserve(4);
  v.push_back(nullptr);
  auto it = v.insert(v.begin(), nullptr);
  *it = static_cast<int*>(malloc(sizeof(int)));
}

void emplace_ok(std::vector<int*>& v) {
  auto it = v.emplace(v.begin(), nullptr);
  *it = static_cast<int*>(malloc(sizeof(int)));
}

void insert_count_ok(std::vector<int*>& v) {
  auto it = v.insert(v.begin(), 1, nullptr);
  *it = static_cast<int*>(malloc(sizeof(int)));
}

void insert_initializer_list_ok(std::vector<int*>& v) {
  auto it = v.insert(v.begin(), {nullptr});
  *it = static_cast<int*>(malloc(sizeof(int)));
}

void local_insert_bad() {
  std::vector<int*> v;
  auto it = v.insert(v.begin(), nullptr);
  *it = static_cast<int*>(malloc(sizeof(int)));
}

void local_emplace_bad() {
  std::vector<int*> v;
  auto it = v.emplace(v.begin(), nullptr);
  *it = static_cast<int*>(malloc(sizeof(int)));
}

void local_insert_count_bad() {
  std::vector<int*> v;
  auto it = v.insert(v.begin(), 1, nullptr);
  *it = static_cast<int*>(malloc(sizeof(int)));
}

void local_insert_initializer_list_bad() {
  std::vector<int*> v;
  auto it = v.insert(v.begin(), {nullptr});
  *it = static_cast<int*>(malloc(sizeof(int)));
}

void deleted_pointee_bad(std::vector<int*>& v) {
  auto it = v.insert(v.begin(), nullptr);
  int* p = new int(1);
  *it = p;
  delete p;
  **it = 2;
}

int returned_iterator_invalidated_bad(std::vector<int>& v) {
  auto it = v.insert(v.begin(), 1);
  v.clear();
  return *it;
}

// Inserting before an existing element invalidates its iterator even with spare
// capacity. Binding the returned iterator must not resurrect that old iterator.
int old_iterator_invalidated_bad() {
  std::vector<int> v;
  v.reserve(4);
  v.push_back(1);
  auto old = v.begin();
  v.insert(v.begin(), 2);
  return *old;
}

int old_reference_reallocated_bad() {
  std::vector<int> v;
  v.reserve(1);
  v.push_back(1);
  int* old = &v[0];
  v.insert(v.end(), 2);
  return *old;
}

int spare_end_keeps_reference_ok() {
  std::vector<int> v;
  v.reserve(4);
  v.push_back(1);
  int* old = &v[0];
  v.insert(v.end(), 2);
  return *old;
}

void inserted_size_ok(std::vector<int>& v) {
  v.clear();
  v.reserve(4);
  v.insert(v.end(), 1);
  if (v.empty()) {
    int* p = nullptr;
    *p = 1;
  }
}

void plain_leak_bad() { malloc(sizeof(int)); }

} // namespace vector_insert_iterator
