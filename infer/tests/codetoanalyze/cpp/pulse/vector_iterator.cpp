/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
#include <iostream>
#include <vector>

void iterator_read_after_emplace_bad(std::vector<int>& vec) {
  auto iter = vec.begin();
  vec.emplace(iter, 4);
  std::cout << *iter << '\n';
}

void iterator_next_after_emplace_bad(std::vector<int>& vec) {
  auto iter = vec.begin();
  vec.emplace(iter, 4);
  ++iter;
  std::cout << *iter << '\n';
}

void another_iterator_ok(std::vector<int>& vec) {
  auto iter = vec.begin();
  vec.emplace(iter, 4);
  auto another_iter = vec.begin();
  std::cout << *another_iter << '\n';
  ++another_iter;
  std::cout << *another_iter << '\n';
}

void read_iterator_loop_ok(std::vector<int>& vec) {
  int sum = 0;
  for (auto iter = vec.begin(); iter != vec.end(); ++iter) {
    sum += *iter;
  }
}

void iterator_next_after_emplace_loop_latent(std::vector<int>& vec) {
  int sum = 0;
  for (auto iter = vec.begin(); iter != vec.end(); ++iter) {
    int elem = *iter;
    sum += elem;
    if (elem < 0)
      vec.emplace(iter, -elem);
  }
}

void iterator_after_push_back_loop_bad(std::vector<int>& vec_other) {
  std::vector<int> vec(2);
  auto iter_begin = vec.begin();
  auto iter_end = vec.end();
  for (const auto& i : vec_other) {
    vec.push_back(i);
  }
  int sum = 0;
  for (auto iter = iter_begin; iter != iter_end; ++iter) {
    sum += *iter;
  }
}

void FN_iterator_empty_vector_read_bad() {
  std::vector<int> vec = {};
  auto iter = vec.begin();
  std::cout << *iter << '\n';
}

void iterator_end_read_bad() {
  std::vector<int> vec = {1, 2};
  auto iter = vec.end();
  std::cout << *iter << '\n';
}

void iterator_end_next_bad() {
  std::vector<int> vec = {1, 2};
  auto iter = vec.end();
  ++iter;
}

void iterator_end_prev_read_ok() {
  std::vector<int> vec = {1, 2};
  auto iter = vec.end();
  std::cout << *(--iter) << '\n';
}

void iterator_end_prev_next_read_bad(std::vector<int>& vec) {
  auto iter = vec.end();
  --iter;
  ++iter;
  std::cout << *iter << '\n';
}

void iterator_prev_after_emplace_bad(std::vector<int>& vec) {
  auto iter = vec.begin();
  ++iter;
  vec.emplace(iter, 4);
  --iter;
  std::cout << *iter << '\n';
}

void FN_iterator_begin_prev_read_bad() {
  std::vector<int> vec = {1, 2};
  auto iter = vec.begin();
  std::cout << *(--iter) << '\n';
}

bool for_each_ok(std::vector<int>& vec, bool b) {
  int res = 0;
  for (const auto& elem : vec) {
    res += 0;
  }
  return b;
}

void call_iterator_loop_ok(bool b) {
  std::vector<int> vec;
  bool finished = false;
  while (!finished) {
    if (!for_each_ok(vec, b))
      return;
  }
}

std::vector<int>::iterator find(std::vector<int>& vec, bool b) {
  for (auto it = vec.begin(); it != vec.end(); ++it) {
    if (b) {
      return it;
    }
  }
  return vec.end();
}

void iterator_end_returned_ok(std::vector<int> vec, bool b) {
  auto it = find(vec, b);
  if (it != vec.end()) {
    *it = 3;
  } else {
    return;
  }
}

void iterator_read_after_erase_bad(std::vector<int>& vec) {
  auto iter = vec.begin();
  vec.erase(iter);
  std::cout << *iter << '\n';
}

void iterator_next_after_erase_bad(std::vector<int>& vec) {
  auto iter = vec.begin();
  vec.erase(iter);
  ++iter;
}

void iterator_returned_by_erase_ok(std::vector<int>& vec) {
  auto iter = vec.begin();
  iter = vec.erase(iter);
  if (iter != vec.end()) {
    std::cout << *iter << '\n';
  }
}

void erase_loop_ok(std::vector<int>& vec) {
  for (auto iter = vec.begin(); iter != vec.end();) {
    if (*iter == 0) {
      iter = vec.erase(iter);
    } else {
      ++iter;
    }
  }
}

void erase_keeps_earlier_reference_ok(std::vector<int>& vec) {
  int* elt = &vec[0];
  auto iter = vec.begin();
  ++iter;
  vec.erase(iter);
  std::cout << *elt << '\n';
}

void erase_then_read_later_reference_bad(std::vector<int>& vec) {
  int* elt = &vec[2];
  auto iter = vec.begin();
  ++iter;
  vec.erase(iter);
  std::cout << *elt << '\n';
}

void erase_keeps_previous_iterator_ok(std::vector<int>& vec) {
  auto prev = vec.begin();
  auto iter = vec.begin();
  ++iter;
  iter = vec.erase(iter);
  std::cout << *prev << '\n';
}

struct Extent {
  int start;
  int length;
};

void erase_merge_adjacent_loop_ok(std::vector<Extent>& extents) {
  if (extents.empty()) {
    return;
  }
  auto prev = extents.begin();
  auto iter = extents.begin();
  for (++iter; iter != extents.end();) {
    if (iter->start == prev->start + prev->length) {
      prev->length += iter->length;
      iter = extents.erase(iter);
      continue;
    }
    prev = iter;
    ++iter;
  }
}

// an iterator after the erased position is only invalidated if its element
// was accessed before the erase
void FN_iterator_after_erased_position_read_bad(std::vector<int>& vec) {
  auto iter = vec.begin();
  ++iter;
  auto later = iter;
  ++later;
  vec.erase(iter);
  std::cout << *later << '\n';
}

// the position of end() is not related to the one of begin()
void FP_iterator_before_erased_position_read_ok(std::vector<int>& vec) {
  auto iter = vec.begin();
  vec.erase(vec.end() - 1);
  std::cout << *iter << '\n';
}

// comparing iterators does not check that they are still valid
void FN_cached_end_across_erase_bad(std::vector<int>& vec) {
  auto end = vec.end();
  for (auto iter = vec.begin(); iter != end;) {
    if (*iter == 0) {
      iter = vec.erase(iter);
    } else {
      ++iter;
    }
  }
}

// the position returned by erase is unknown
void FN_iterator_returned_by_erase_last_read_bad(std::vector<int>& vec) {
  auto iter = vec.erase(vec.end() - 1);
  std::cout << *iter << '\n';
}

void iterator_returned_by_erase_after_push_back_bad(std::vector<int>& vec) {
  auto iter = vec.erase(vec.begin());
  vec.push_back(4);
  std::cout << *iter << '\n';
}

void iterator_returned_by_insert_ok(std::vector<int>& vec) {
  auto iter = vec.begin();
  iter = vec.insert(iter, 4);
  std::cout << *iter << '\n';
}

void iterator_reassigned_after_push_back_ok(std::vector<int>& vec) {
  auto iter = vec.begin();
  vec.push_back(4);
  iter = vec.begin();
  std::cout << *iter << '\n';
}

void iterator_assigned_then_push_back_bad(std::vector<int>& vec,
                                          std::vector<int>& vec_other) {
  auto iter = vec_other.begin();
  iter = vec.begin();
  vec.push_back(4);
  std::cout << *iter << '\n';
}

struct IteratorElem {
  int x;
};

int default_iterator_assigned_ok(std::vector<IteratorElem>& vec) {
  std::vector<IteratorElem>::iterator iter;
  iter = vec.begin();
  return iter->x;
}

int iterator_assign_copies_base_ok(std::vector<int>::iterator iter) {
  std::vector<int>::iterator iter_copy;
  iter_copy = iter;
  if (iter_copy.base() != iter.base()) {
    int* q = nullptr;
    return *q;
  }
  return 0;
}

int iterator_assign_then_advance_ok(std::vector<int>::iterator iter) {
  std::vector<int>::iterator iter_copy;
  iter_copy = iter;
  iter_copy += 1;
  if (iter_copy.base() == iter.base()) {
    int* q = nullptr;
    return *q;
  }
  return 0;
}

int iterator_assign_then_next_ok() {
  std::vector<int> vec = {1, 2};
  auto iter = vec.begin();
  std::vector<int>::iterator next;
  next = iter;
  ++next;
  *next = 1;
  *iter = 0;
  if (*next == 0) {
    int* q = nullptr;
    return *q;
  }
  return 0;
}

void iterator_end_prev_then_read_ok() {
  std::vector<int> vec = {1, 2};
  auto iter = vec.end();
  --iter;
  std::cout << *iter << '\n';
}

int iterator_assign_end_then_prev_loop_ok(std::vector<int>& vec) {
  int sum = 0;
  std::vector<int>::iterator iter;
  iter = vec.end();
  while (iter != vec.begin()) {
    --iter;
    sum += *iter;
  }
  return sum;
}

void iterator_end_minus_one_read_ok(std::vector<int>& vec) {
  if (vec.empty()) {
    return;
  }
  std::cout << *(vec.end() - 1) << '\n';
}

void iterator_end_minus_assign_read_ok(std::vector<int>& vec) {
  if (vec.empty()) {
    return;
  }
  auto iter = vec.end();
  iter -= 1;
  std::cout << *iter << '\n';
}

void iterator_plus_after_push_back_bad(std::vector<int>& vec) {
  auto iter = vec.begin() + 1;
  vec.push_back(4);
  std::cout << *iter << '\n';
}

void iterator_plus_assign_after_push_back_bad(std::vector<int>& vec) {
  auto iter = vec.begin();
  iter += 1;
  vec.push_back(4);
  std::cout << *iter << '\n';
}

int iterator_plus_base_ok(std::vector<int>::iterator iter) {
  if ((iter + 2).base() != iter.base() + 2) {
    int* q = nullptr;
    return *q;
  }
  return 0;
}

int iterator_cbegin_loop_ok(const std::vector<int>& vec) {
  int sum = 0;
  for (auto iter = vec.cbegin(); iter != vec.cend(); ++iter) {
    sum += *iter;
  }
  return sum;
}

void iterator_cend_read_bad() {
  std::vector<int> vec = {1, 2};
  auto iter = vec.cend();
  std::cout << *iter << '\n';
}

void iterator_end_plus_zero_read_bad(std::vector<int>& vec) {
  std::cout << *(vec.end() + 0) << '\n';
}

void iterator_end_minus_then_plus_read_bad(std::vector<int>& vec) {
  auto iter = vec.end() - 1;
  std::cout << *(iter + 1) << '\n';
}

int iterator_plus_minus_assign_round_trip_ok(std::vector<int>& vec) {
  auto iter = vec.begin();
  iter += 2;
  iter -= 2;
  if (iter != vec.begin()) {
    int* q = nullptr;
    return *q;
  }
  return 0;
}

int iterator_plus_same_offset_ok(std::vector<int>& vec) {
  if (vec.begin() + 1 != vec.begin() + 1) {
    int* q = nullptr;
    return *q;
  }
  return 0;
}

int iterator_arrow_after_push_back_bad(std::vector<IteratorElem>& vec) {
  auto iter = vec.begin();
  vec.push_back(IteratorElem{1});
  return iter->x;
}

int iterator_end_arrow_bad(std::vector<IteratorElem>& vec) {
  return vec.end()->x;
}

struct IteratorNode {
  bool defined;
};

IteratorNode* iterator_arrow_loop_until_defined_ok(
    std::vector<std::pair<IteratorNode*, IteratorNode*>>& vec) {
  auto iter = vec.begin();
  while (iter != vec.end() && !iter->first->defined) {
    ++iter;
  }
  return iter == vec.end() ? nullptr : iter->second;
}

void erase_keeps_iterator_before_offset_position_ok(std::vector<int>& vec) {
  auto prev = vec.begin();
  vec.erase(vec.begin() + 1);
  std::cout << *prev << '\n';
}
