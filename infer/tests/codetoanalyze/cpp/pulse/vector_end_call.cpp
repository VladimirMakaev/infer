/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
#include "vector_end_call.h"
#include <deque>
#include <iterator>
#include <list>
#include <map>
#include <utility>

namespace vector_end_call {
using Vector = std::vector<int>;
using Iterator = Vector::iterator;

int read(Iterator it) { return *it; }
int read_reference(const Iterator& it) { return *it; }
int read_guarded(Iterator it, bool should_read) {
  if (should_read)
    return *it;
  return 0;
}
int read_twice_removed(Iterator it) { return read(it); }
Iterator copy(Iterator it) { return it; }
bool compare(Iterator left, Iterator right) { return left == right; }
void ignore(Iterator it) {}
int decrement_and_read(Iterator it) {
  --it;
  return *it;
}
int increment_and_read(Iterator it) {
  ++it;
  return *it;
}
int rebind_and_read(Iterator it, Vector& vec) {
  it = vec.begin();
  return *it;
}

// Reallocation and clear already report through a call. A valid past-end
// position also requires a check when there is no invalidating event.
int grow_before_helper_bad() {
  Vector vec{7};
  auto end = vec.end();
  vec.reserve(2);
  return read(end);
}
int noop_reserve_before_helper_bad() {
  Vector vec{7};
  auto end = vec.end();
  vec.reserve(1);
  return read(end);
}
int end_through_helper_bad() {
  Vector vec{7};
  return read(vec.end());
}
int clear_before_helper_bad() {
  Vector vec{7};
  auto end = vec.end();
  vec.clear();
  return read(end);
}
int end_inline_bad() {
  Vector vec{7};
  return *vec.end();
}
int grow_inline_bad() {
  Vector vec{7};
  auto end = vec.end();
  vec.reserve(2);
  return *end;
}
int end_two_helpers_bad() {
  Vector vec{7};
  return read_twice_removed(vec.end());
}
int end_separate_tu_bad() {
  Vector vec{7};
  return read_separate(vec.end());
}
int end_const_reference_bad() {
  Vector vec{7};
  auto end = vec.end();
  return read_reference(end);
}
int copied_end_through_helper_bad() {
  Vector vec{7};
  auto end = copy(vec.end());
  return read(end);
}
int increment_end_through_helper_bad() {
  Vector vec{7};
  return increment_and_read(vec.end());
}
int offset_back_to_end_inline_bad() {
  Vector vec{7};
  auto it = vec.end() - 1;
  ++it;
  return *it;
}
int offset_back_to_end_through_helper_bad() {
  Vector vec{7};
  auto it = vec.end() - 1;
  ++it;
  return read(it);
}
int guarded_end_through_helper_bad() {
  Vector vec{7};
  return read_guarded(vec.end(), true);
}

// Receiving an end value imposes no dereference requirement on its own.
void copy_end_ok() {
  Vector vec{7};
  auto end = copy(vec.end());
  ignore(end);
}
bool compare_end_ok() {
  Vector vec{7};
  return compare(vec.end(), vec.end());
}
void ignore_end_ok() {
  Vector vec{7};
  ignore(vec.end());
}
int distance_to_end_ok() {
  Vector vec{7};
  return std::distance(vec.begin(), copy(vec.end()));
}
int begin_through_helper_ok() {
  Vector vec{7};
  return read(vec.begin());
}
int guarded_end_not_read_ok() {
  Vector vec{7};
  return read_guarded(vec.end(), false);
}
int guarded_begin_read_ok() {
  Vector vec{7};
  return read_guarded(vec.begin(), true);
}
int begin_separate_tu_ok() {
  Vector vec{7};
  return read_separate(vec.begin());
}
int begin_const_reference_ok() {
  Vector vec{7};
  auto begin = vec.begin();
  return read_reference(begin);
}
int decrement_end_before_helper_ok() {
  Vector vec{7};
  auto end = vec.end();
  --end;
  return read(end);
}
int decrement_end_in_helper_ok() {
  Vector vec{7};
  return decrement_and_read(vec.end());
}
int end_minus_one_through_helper_ok() {
  Vector vec{7};
  return read(vec.end() - 1);
}
int replace_end_with_begin_ok() {
  Vector vec{7};
  return rebind_and_read(vec.end(), vec);
}
int swap_end_with_begin_ok() {
  Vector vec{7};
  auto it = vec.end();
  auto begin = vec.begin();
  std::swap(it, begin);
  return read(it);
}
int empty_loop_ok() {
  Vector vec;
  int total = 0;
  for (auto it = vec.begin(); !compare(it, vec.end()); ++it)
    total += read(it);
  return total;
}
int nonempty_loop_ok() {
  Vector vec{7};
  int total = 0;
  for (auto it = vec.begin(); !compare(it, vec.end()); ++it)
    total += read(it);
  return total;
}

template <typename It> int read_other(It it) { return *it; }
template <typename It> bool compare_other(It left, It right) {
  return left == right;
}
int list_begin_through_helper_ok() {
  std::list<int> values{7};
  return read_other(values.begin());
}
bool list_compare_end_ok() {
  std::list<int> values{7};
  return compare_other(values.end(), values.end());
}
int deque_begin_through_helper_ok() {
  std::deque<int> values{7};
  return read_other(values.begin());
}
int read_map(std::map<int, int>::iterator it) { return it->second; }
int map_begin_through_helper_ok() {
  std::map<int, int> values{{1, 7}};
  return read_map(values.begin());
}
} // namespace vector_end_call
