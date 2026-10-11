/* Copyright (c) Meta Platforms, Inc. and affiliates.
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree. */
#include <vector>
#include <new>
#include <utility>

namespace vector_reserve_model {
int zero_ok() {
  std::vector<int> v;
  v.push_back(7);
  int* p = &v[0];
  v.reserve(0);
  return *p;
}

int repeated_zero_ok() {
  std::vector<int> v;
  v.reserve(0);
  v.push_back(7);
  int* p = &v[0];
  v.reserve(0);
  v.reserve(0);
  return *p;
}

int within_capacity_ok() {
  std::vector<int> v;
  v.reserve(8);
  v.push_back(7);
  int* p = &v[0];
  v.reserve(4);
  v.reserve(0);
  return *p;
}

// Existing limitation: computed requests retain conservative invalidation.
int FP_equal_size_ok() {
  std::vector<int> v;
  v.push_back(7);
  int* p = &v[0];
  v.reserve(v.size());
  return *p;
}

int clear_keeps_capacity_ok() {
  std::vector<int> v;
  v.reserve(8);
  v.push_back(7);
  v.clear();
  v.push_back(9);
  int* p = &v[0];
  v.reserve(4);
  return *p;
}

int shrink_then_zero_ok() {
  std::vector<int> v;
  v.push_back(7);
  v.shrink_to_fit();
  int* p = &v[0];
  v.reserve(0);
  return *p;
}

int shrink_then_size_ok() {
  std::vector<int> v;
  v.push_back(7);
  v.shrink_to_fit();
  int* p = &v[0];
  v.reserve(1);
  return *p;
}

int grow_then_zero_bad() {
  std::vector<int> v;
  v.reserve(1);
  v.push_back(7);
  int* p = &v[0];
  v.push_back(9);
  v.reserve(0);
  return *p;
}

int zero_does_not_reserve_growth_bad() {
  std::vector<int> v;
  v.reserve(0);
  v.push_back(7);
  int* p = &v[0];
  v.push_back(9);
  return *p;
}

int force_growth_bad() {
  std::vector<int> v;
  v.push_back(7);
  int* p = &v[0];
  v.reserve(v.capacity() + 1);
  return *p;
}

int resize_then_zero_bad() {
  std::vector<int> v;
  v.reserve(1);
  v.push_back(7);
  int* p = &v[0];
  v.resize(2);
  v.reserve(0);
  return *p;
}

void grow_in_callee(std::vector<int>& v) { v.push_back(9); }

// Existing limitation: the callee growth summary still assumes reserve was sufficient.
int FN_callee_growth_then_zero_bad() {
  std::vector<int> v;
  v.reserve(1);
  v.push_back(7);
  int* p = &v[0];
  grow_in_callee(v);
  v.reserve(0);
  return *p;
}

int unknown_growth_then_zero_bad(int count) {
  std::vector<int> v;
  v.reserve(1);
  v.push_back(7);
  int* p = &v[0];
  v.resize(count);
  v.reserve(0);
  return *p;
}

// Unknown capacity remains conservative; a larger reserve can invalidate p.
int unknown_reserve_bad(std::vector<int>& v) {
  int* p = &v[0];
  v.reserve(v.size() + 1);
  return *p;
}
int reconstruct_initializer_list_bad() {
  alignas(std::vector<int>) char storage[sizeof(std::vector<int>)];
  std::vector<int>* v = new (storage) std::vector<int>();
  v->reserve(8);
  v->~vector();
  v = new (storage) std::vector<int>{7};
  int* p = &(*v)[0];
  v->reserve(2);
  int value = *p;
  v->~vector();
  return value;
}
int derived_request_bad() {
  std::vector<int> v;
  v.push_back(7);
  std::vector<int> other{9};
  int* p = &v[0];
  v.reserve(v.size() + other.size());
  return *p;
}
int populated_growth_then_zero_bad() {
  std::vector<int> v{7};
  v.reserve(1);
  int* p = &v[0];
  v.push_back(9);
  v.reserve(0);
  return *p;
}
int computed_resize_then_zero_bad() {
  std::vector<int> v;
  v.reserve(1);
  v.push_back(7);
  std::vector<int> other{9};
  int* p = &v[0];
  v.resize(other.size());
  v.push_back(9);
  v.reserve(0);
  return *p;
}
void replace_in_callee(std::vector<int>& v) { v = std::vector<int>{7}; }
int replaced_then_reserve_bad() {
  std::vector<int> v;
  v.reserve(8);
  v.push_back(7);
  replace_in_callee(v);
  int* p = &v[0];
  v.reserve(2);
  return *p;
}
void move_in_callee(std::vector<int>& v) { std::vector<int> dst{std::move(v)}; }
int moved_source_then_reserve_bad() {
  std::vector<int> v;
  v.reserve(8);
  v.push_back(7);
  move_in_callee(v);
  v.push_back(9);
  int* p = &v[0];
  v.reserve(2);
  return *p;
}
void unknown_mutation(std::vector<int>&);
int unknown_mutation_then_reserve_bad() {
  std::vector<int> v;
  v.reserve(8);
  v.push_back(7);
  unknown_mutation(v);
  int* p = &v[0];
  v.reserve(4);
  return *p;
}
} // namespace vector_reserve_model
