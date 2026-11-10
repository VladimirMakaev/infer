/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
#include <deque>
#include <list>
#include <map>

// These empty operations are undefined, but must not hide a separate later defect.
int known_empty_list_pop_front_then_null_bad() {
  std::list<int> values;
  values.pop_front();
  return *static_cast<int*>(nullptr);
}

int list_emptied_by_first_pop_then_null_bad(std::list<int>& values) {
  if (values.size() != 1) {
    return 0;
  }
  values.pop_front();
  values.pop_front();
  return *static_cast<int*>(nullptr);
}

void known_empty_list_popper() {
  std::list<int> values;
  values.pop_front();
}

int caller_of_known_empty_popper_bad() {
  known_empty_list_popper();
  return *static_cast<int*>(nullptr);
}

void known_empty_list_popper_outer() { known_empty_list_popper(); }

int caller_two_levels_after_known_empty_pop_bad() {
  known_empty_list_popper_outer();
  return *static_cast<int*>(nullptr);
}

int known_empty_list_front_then_null_bad() {
  std::list<int> values;
  (void)values.front();
  return *static_cast<int*>(nullptr);
}

int known_empty_list_back_then_null_bad() {
  std::list<int> values;
  (void)values.back();
  return *static_cast<int*>(nullptr);
}

int known_empty_list_fronter() {
  std::list<int> values;
  return values.front();
}

int caller_of_known_empty_fronter_bad() {
  int value = known_empty_list_fronter();
  return *static_cast<int*>(nullptr) + value;
}

int known_empty_deque_pop_front_then_null_bad() {
  std::deque<int> values;
  values.pop_front();
  return *static_cast<int*>(nullptr);
}

int known_empty_deque_pop_back_then_null_bad() {
  std::deque<int> values;
  values.pop_back();
  return *static_cast<int*>(nullptr);
}

// An empty pop must not leave a negative size that makes empty() discard the path again.
int repeated_empty_pops_then_empty_check_bad() {
  std::list<int> values;
  values.pop_front();
  values.pop_back();
  values.pop_front();
  if (values.empty()) {
    return *static_cast<int*>(nullptr);
  }
  return 0;
}

// The undefined operations themselves do not introduce a new diagnostic.
void known_empty_container_calls_ok() {
  std::list<int> values;
  (void)values.front();
  (void)values.back();
  values.pop_front();
  values.pop_back();
}

int nonempty_list_pop_then_null_bad() {
  std::list<int> values{1};
  values.pop_front();
  return *static_cast<int*>(nullptr);
}

int nonempty_deque_pop_then_null_bad() {
  std::deque<int> values{1};
  values.pop_back();
  return *static_cast<int*>(nullptr);
}

// The size is symbolic here. Its precondition and the report's latency must stay unchanged.
int symbolic_list_pop_then_null_bad(std::list<int>& values) {
  values.pop_front();
  return *static_cast<int*>(nullptr);
}

int symbolic_list_front_then_null_bad(std::list<int>& values) {
  (void)values.front();
  return *static_cast<int*>(nullptr);
}

int symbolic_list_back_then_null_bad(std::list<int>& values) {
  (void)values.back();
  return *static_cast<int*>(nullptr);
}

// Unlike front()/back(), at() on an empty map throws; its existing path handling stays exact.
int known_empty_map_at_then_null_ok() {
  std::map<int, int> values;
  (void)values.at(0);
  return *static_cast<int*>(nullptr);
}
