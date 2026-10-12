/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace span_model {

// A span views memory it does not own, so it dangles once the owner releases
// it.

int dangling_after_vector_destroyed_bad() {
  std::span<int> s;
  {
    std::vector<int> v = {1, 2, 3};
    s = std::span<int>(v);
  }
  return s[0];
}

int dangling_after_push_back_bad(std::vector<int>& v) {
  std::span<int> s(v);
  v.push_back(42);
  return s[0];
}

int dangling_after_clear_bad(std::vector<int>& v) {
  std::span<int> s(v);
  v.clear();
  return s[0];
}

int dangling_copy_bad() {
  std::span<int> s;
  {
    std::vector<int> v = {1, 2, 3};
    std::span<int> t(v);
    s = t;
  }
  return s[0];
}

int dangling_data_bad() {
  std::span<int> s;
  {
    std::vector<int> v = {1, 2, 3};
    s = std::span<int>(v);
  }
  return *s.data();
}

int dangling_front_bad() {
  std::span<int> s;
  {
    std::vector<int> v = {1, 2, 3};
    s = std::span<int>(v);
  }
  return s.front();
}

int dangling_subspan_bad(std::vector<int>& v) {
  std::span<int> s(v);
  auto t = s.subspan(1);
  v.push_back(42);
  return t[0];
}

int span_over_live_vector_ok(std::vector<int>& v) {
  std::span<int> s(v);
  if (s.empty()) {
    return 0;
  }
  return s[0];
}

int span_rebuilt_after_push_back_ok(std::vector<int>& v) {
  std::span<int> s(v);
  v.push_back(42);
  s = std::span<int>(v);
  return s[0];
}

int span_over_local_array_ok() {
  int a[3] = {1, 2, 3};
  std::span<int> s(a);
  return s[0];
}

// The extent of the view bounds the subviews taken from it.

int subspan_offset_out_of_bounds_bad() {
  int a[3] = {1, 2, 3};
  std::span<int> s(a);
  auto t = s.subspan(5);
  return t[0];
}

int subspan_count_out_of_bounds_bad() {
  int a[3] = {1, 2, 3};
  std::span<int> s(a);
  auto t = s.subspan(1, 5);
  return t[0];
}

int first_out_of_bounds_bad() {
  int a[3] = {1, 2, 3};
  std::span<int> s(a);
  return s.first(5)[0];
}

int last_out_of_bounds_bad() {
  int a[3] = {1, 2, 3};
  std::span<int> s(a);
  return s.last(5)[0];
}

int subspan_of_vector_out_of_bounds_bad() {
  std::vector<int> v = {1, 2, 3};
  std::span<int> s(v);
  return s.subspan(7)[0];
}

int subspan_of_std_array_out_of_bounds_bad() {
  std::array<int, 3> a = {1, 2, 3};
  std::span<int> s(a);
  return s.subspan(7)[0];
}

int subspan_of_pointer_and_count_out_of_bounds_bad(int* p) {
  std::span<int> s(p, 3);
  return s.subspan(7)[0];
}

int subviews_in_bounds_ok() {
  int a[4] = {1, 2, 3, 4};
  std::span<int> s(a);
  return s.subspan(1)[0] + s.subspan(1, 2)[0] + s.first(2)[0] + s.last(2)[0];
}

int subview_of_subview_ok() {
  int a[4] = {1, 2, 3, 4};
  std::span<int> s(a);
  return s.subspan(1).first(2)[0];
}

// The extent of a span built from a pointer and a count, or from a vector of
// unknown size, is not a constant, so no bounds claim is made about it.
int subspan_of_unknown_extent_ok(int* p, std::size_t n) {
  std::span<int> s(p, n);
  return s.subspan(4)[0];
}

// `size()` agrees with what the span was built from.

std::size_t sizes_ok(std::vector<int>& v) {
  int a[4] = {1, 2, 3, 4};
  std::array<int, 5> b = {1, 2, 3, 4, 5};
  std::span<int> s1(a);
  std::span<int> s2(b);
  std::span<int> s3(v);
  std::span<int, 4> s4(a);
  std::span<int> s5;
  return s1.size() + s2.size() + s3.size() + s4.size() + s5.size();
}

int empty_span_is_empty_bad() {
  std::span<int> s;
  if (s.empty()) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}

int size_of_subspan_bad() {
  int a[4] = {1, 2, 3, 4};
  std::span<int> s(a);
  if (s.subspan(1).size() == 3) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}

// Known limitations.

// Element access is not bounds checked, as for `std::vector::operator[]`.
int FN_element_out_of_bounds_bad() {
  int a[3] = {1, 2, 3};
  std::span<int> s(a);
  return s[5];
}

// Only constant extents are compared, so an extent that is only known
// symbolically is not checked.
int FN_subspan_of_symbolic_extent_bad(std::vector<int>& v) {
  std::span<int> s(v);
  return s.subspan(v.size() + 1)[0];
}

} // namespace span_model
