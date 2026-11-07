/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <algorithm>
#include <array>
#include <cstddef>
#include <initializer_list>
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

// A span over an EMPTY range. Its data pointer is whatever the caller passed,
// and for an empty span that is routinely null -- an empty
// `std::initializer_list` hands over exactly that. Forming and comparing the
// iterators must not require that pointer to be dereferenceable: `begin()` of
// an empty span is its `end()`, so the loop body is never entered.

bool has_key(int value, std::span<const int> allowed) {
  for (int key : allowed) {
    if (key == value) {
      return true;
    }
  }
  return false;
}

bool has_key(int value, std::initializer_list<int> allowed) {
  return has_key(value, std::span<const int>(allowed.begin(), allowed.size()));
}

bool iterate_empty_span_ok(int value) { return has_key(value, {}); }

bool iterate_nonempty_span_ok(int value) { return has_key(value, {1, 2}); }

// The extent here is a constant zero, so the loop is provably empty.
int loop_over_zero_extent_span(const int* p) {
  std::span<const int> s(p, std::size_t{0});
  int n = 0;
  for (auto it = s.begin(); it != s.end(); ++it) {
    n++;
  }
  return n;
}

int loop_over_zero_extent_span_ok() { return loop_over_zero_extent_span(nullptr); }

// The controls. A non-zero extent does promise storage, so reading through it
// is still reported.

int deref_begin_of_span(const int* p, std::size_t n) {
  std::span<const int> s(p, n);
  return *s.begin();
}

int deref_begin_of_null_span_bad() { return deref_begin_of_span(nullptr, 3); }

int deref_begin_of_nonempty_span_ok() {
  int a[3] = {1, 2, 3};
  return deref_begin_of_span(a, 3);
}

// Stepping past the one element an empty span does not have is undefined even
// though the loop above is not entered.
int increment_begin_of_span(std::initializer_list<int> a) {
  std::span<const int> s(a.begin(), a.size());
  auto it = s.begin();
  ++it;
  return 0;
}

int increment_begin_of_empty_span_bad() { return increment_begin_of_span({}); }

// Holding BOTH iterators of a span whose extent is symbolic, then reading
// through the first one. Neither helper is wrong for every caller -- the extent
// is the caller's to choose -- so neither may report here, in either call order
// and whether or not an algorithm is handed the pair.

int begin_then_end_then_deref_ok(const int* p, std::size_t n) {
  std::span<const int> s(p, n);
  auto it = s.begin();
  (void)s.end();
  return *it;
}

int end_then_begin_then_deref_ok(const int* p, std::size_t n) {
  std::span<const int> s(p, n);
  (void)s.end();
  auto it = s.begin();
  return *it;
}

int sort_then_deref_ok(int* p, std::size_t n) {
  std::span<int> s(p, n);
  auto b = s.begin();
  auto e = s.end();
  std::sort(b, e);
  return *b;
}

// An extent the caller fixed at zero is a different matter. That span really is
// empty, so both of its iterators are past the end and reading through either
// one is an error worth reporting.

int deref_end_of_empty_span_bad() {
  static const int a[1] = {7};
  std::span<const int> s(a, std::size_t{0});
  auto e = s.end();
  return *e;
}

int deref_begin_of_empty_span_bad() {
  static const int a[1] = {7};
  std::span<const int> s(a, std::size_t{0});
  return *s.begin();
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
