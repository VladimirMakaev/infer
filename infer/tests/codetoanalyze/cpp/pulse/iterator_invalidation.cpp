/* Copyright (c) Meta Platforms, Inc. and affiliates.
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree. */
#include <vector>
#include <iterator>
#include <utility>
#if __cplusplus >= 201703L
namespace iterator_invalidation {
int as_const_live_ok() {
  std::vector<int> v{7};
  auto it = std::as_const(v).begin();
  return *it;
}
int as_const_clear_bad() {
  std::vector<int> v{7};
  auto it = std::as_const(v).begin();
  v.clear();
  return *it;
}
int as_const_cbegin_clear_bad() {
  std::vector<int> v{7};
  auto it = std::as_const(v).cbegin();
  v.clear();
  return *it;
}
int const_reference_clear_bad() {
  std::vector<int> v{7};
  const std::vector<int>& r = v;
  auto it = r.begin();
  v.clear();
  return *it;
}
int mutable_clear_bad() {
  std::vector<int> v{7};
  auto it = v.begin();
  v.clear();
  return *it;
}
int as_const_alias_update_ok() {
  std::vector<int> v;
  v.reserve(8);
  v.push_back(7);
  const auto& r = std::as_const(v);
  v.push_back(9);
  if (r.size() != 2) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}
} // namespace iterator_invalidation
#endif

// [vector.modifiers]: erase invalidates at/after its position. End insertion
// with spare capacity preserves element iterators. Equivalent begin/end-derived
// positions must agree.
namespace iterator_invalidation {
static std::vector<int> local_reserved() {
  std::vector<int> v;
  v.reserve(8);
  v.push_back(7);
  v.push_back(8);
  v.push_back(9);
  return v;
}
static std::vector<int> list_reserved() {
  std::vector<int> v{7, 8, 9};
  v.reserve(8);
  return v;
}
int local_erase_begin0_first_bad() {
  std::vector<int> v;
  v.reserve(8);
  v.push_back(7);
  v.push_back(8);
  v.push_back(9);
  auto it = v.begin();
  v.erase(v.begin());
  return *it;
}
int local_erase_begin0_last_bad() {
  std::vector<int> v;
  v.reserve(8);
  v.push_back(7);
  v.push_back(8);
  v.push_back(9);
  auto it = v.begin() + 2;
  v.erase(v.begin());
  return *it;
}
int local_erase_begin2_first_ok() {
  std::vector<int> v;
  v.reserve(8);
  v.push_back(7);
  v.push_back(8);
  v.push_back(9);
  auto it = v.begin();
  v.erase(v.begin() + 2);
  return *it;
}
int local_erase_begin2_last_bad() {
  std::vector<int> v;
  v.reserve(8);
  v.push_back(7);
  v.push_back(8);
  v.push_back(9);
  auto it = v.begin() + 2;
  v.erase(v.begin() + 2);
  return *it;
}
int local_erase_prev_end_first_ok() {
  std::vector<int> v;
  v.reserve(8);
  v.push_back(7);
  v.push_back(8);
  v.push_back(9);
  auto it = v.begin();
  v.erase(std::prev(v.end()));
  return *it;
}
int local_erase_prev_end_last_bad() {
  std::vector<int> v;
  v.reserve(8);
  v.push_back(7);
  v.push_back(8);
  v.push_back(9);
  auto it = v.begin() + 2;
  v.erase(std::prev(v.end()));
  return *it;
}
int local_erase_end_minus1_first_ok() {
  std::vector<int> v;
  v.reserve(8);
  v.push_back(7);
  v.push_back(8);
  v.push_back(9);
  auto it = v.begin();
  v.erase(v.end() - 1);
  return *it;
}
int local_erase_end_minus1_last_bad() {
  std::vector<int> v;
  v.reserve(8);
  v.push_back(7);
  v.push_back(8);
  v.push_back(9);
  auto it = v.begin() + 2;
  v.erase(v.end() - 1);
  return *it;
}
int local_insert_first_first_bad() {
  std::vector<int> v;
  v.reserve(8);
  v.push_back(7);
  v.push_back(8);
  v.push_back(9);
  auto it = v.begin();
  v.insert(v.begin(), 11);
  return *it;
}
int local_insert_first_last_bad() {
  std::vector<int> v;
  v.reserve(8);
  v.push_back(7);
  v.push_back(8);
  v.push_back(9);
  auto it = v.begin() + 2;
  v.insert(v.begin(), 11);
  return *it;
}
int local_insert_end_first_ok() {
  std::vector<int> v;
  v.reserve(8);
  v.push_back(7);
  v.push_back(8);
  v.push_back(9);
  auto it = v.begin();
  v.insert(v.end(), 11);
  return *it;
}
int local_insert_end_last_ok() {
  std::vector<int> v;
  v.reserve(8);
  v.push_back(7);
  v.push_back(8);
  v.push_back(9);
  auto it = v.begin() + 2;
  v.insert(v.end(), 11);
  return *it;
}
int local_push_first_ok() {
  std::vector<int> v;
  v.reserve(8);
  v.push_back(7);
  v.push_back(8);
  v.push_back(9);
  auto it = v.begin();
  v.push_back(11);
  return *it;
}
int local_push_last_ok() {
  std::vector<int> v;
  v.reserve(8);
  v.push_back(7);
  v.push_back(8);
  v.push_back(9);
  auto it = v.begin() + 2;
  v.push_back(11);
  return *it;
}
int returned_erase_begin0_first_bad() {
  auto v = local_reserved();
  auto it = v.begin();
  v.erase(v.begin());
  return *it;
}
int returned_erase_begin0_last_bad() {
  auto v = local_reserved();
  auto it = v.begin() + 2;
  v.erase(v.begin());
  return *it;
}
int returned_erase_begin2_first_ok() {
  auto v = local_reserved();
  auto it = v.begin();
  v.erase(v.begin() + 2);
  return *it;
}
int returned_erase_begin2_last_bad() {
  auto v = local_reserved();
  auto it = v.begin() + 2;
  v.erase(v.begin() + 2);
  return *it;
}
int returned_erase_prev_end_first_ok() {
  auto v = local_reserved();
  auto it = v.begin();
  v.erase(std::prev(v.end()));
  return *it;
}
int returned_erase_prev_end_last_bad() {
  auto v = local_reserved();
  auto it = v.begin() + 2;
  v.erase(std::prev(v.end()));
  return *it;
}
int returned_erase_end_minus1_first_ok() {
  auto v = local_reserved();
  auto it = v.begin();
  v.erase(v.end() - 1);
  return *it;
}
int returned_erase_end_minus1_last_bad() {
  auto v = local_reserved();
  auto it = v.begin() + 2;
  v.erase(v.end() - 1);
  return *it;
}
int returned_insert_first_first_bad() {
  auto v = local_reserved();
  auto it = v.begin();
  v.insert(v.begin(), 11);
  return *it;
}
int returned_insert_first_last_bad() {
  auto v = local_reserved();
  auto it = v.begin() + 2;
  v.insert(v.begin(), 11);
  return *it;
}
int returned_insert_end_first_ok() {
  auto v = local_reserved();
  auto it = v.begin();
  v.insert(v.end(), 11);
  return *it;
}
int returned_insert_end_last_ok() {
  auto v = local_reserved();
  auto it = v.begin() + 2;
  v.insert(v.end(), 11);
  return *it;
}
int returned_push_first_ok() {
  auto v = local_reserved();
  auto it = v.begin();
  v.push_back(11);
  return *it;
}
int returned_push_last_ok() {
  auto v = local_reserved();
  auto it = v.begin() + 2;
  v.push_back(11);
  return *it;
}
int listreturned_erase_begin0_first_bad() {
  auto v = list_reserved();
  auto it = v.begin();
  v.erase(v.begin());
  return *it;
}
int listreturned_erase_begin0_last_bad() {
  auto v = list_reserved();
  auto it = v.begin() + 2;
  v.erase(v.begin());
  return *it;
}
int listreturned_erase_begin2_first_ok() {
  auto v = list_reserved();
  auto it = v.begin();
  v.erase(v.begin() + 2);
  return *it;
}
int listreturned_erase_begin2_last_bad() {
  auto v = list_reserved();
  auto it = v.begin() + 2;
  v.erase(v.begin() + 2);
  return *it;
}
int listreturned_erase_prev_end_first_ok() {
  auto v = list_reserved();
  auto it = v.begin();
  v.erase(std::prev(v.end()));
  return *it;
}
int listreturned_erase_prev_end_last_bad() {
  auto v = list_reserved();
  auto it = v.begin() + 2;
  v.erase(std::prev(v.end()));
  return *it;
}
int listreturned_erase_end_minus1_first_ok() {
  auto v = list_reserved();
  auto it = v.begin();
  v.erase(v.end() - 1);
  return *it;
}
int listreturned_erase_end_minus1_last_bad() {
  auto v = list_reserved();
  auto it = v.begin() + 2;
  v.erase(v.end() - 1);
  return *it;
}
int listreturned_insert_first_first_bad() {
  auto v = list_reserved();
  auto it = v.begin();
  v.insert(v.begin(), 11);
  return *it;
}
int listreturned_insert_first_last_bad() {
  auto v = list_reserved();
  auto it = v.begin() + 2;
  v.insert(v.begin(), 11);
  return *it;
}
int listreturned_insert_end_first_ok() {
  auto v = list_reserved();
  auto it = v.begin();
  v.insert(v.end(), 11);
  return *it;
}
int listreturned_insert_end_last_ok() {
  auto v = list_reserved();
  auto it = v.begin() + 2;
  v.insert(v.end(), 11);
  return *it;
}
int listreturned_push_first_ok() {
  auto v = list_reserved();
  auto it = v.begin();
  v.push_back(11);
  return *it;
}
int listreturned_push_last_ok() {
  auto v = list_reserved();
  auto it = v.begin() + 2;
  v.push_back(11);
  return *it;
}
int erase_returned_ok() {
  auto v = local_reserved();
  auto it = v.erase(v.begin() + 1);
  return *it;
}

int insert_returned_ok() {
  auto v = local_reserved();
  auto it = v.insert(v.end(), 11);
  return *it;
}
int new_end_after_insert_ok() {
  auto v = local_reserved();
  v.insert(v.end(), 11);
  auto it = v.end();
  --it;
  return *it;
}
int two_erasures_keep_prefix_ok() {
  auto v = local_reserved();
  auto it = v.begin();
  v.erase(v.end() - 1);
  v.erase(v.end() - 1);
  return *it;
}
int insert_forced_growth_bad() {
  auto v = local_reserved();
  v.resize(v.capacity());
  auto it = v.begin();
  v.insert(v.end(), 11);
  return *it;
}
// Filling to capacity forces the next push_back to reallocate.
int push_forced_growth_bad() {
  auto v = local_reserved();
  v.resize(v.capacity());
  auto it = v.begin();
  v.push_back(11);
  return *it;
}
int move_growth_ok() {
  auto v = local_reserved();
  auto w = std::move(v);
  auto it = w.begin();
  w.push_back(11);
  return *it;
}
int copied_storage_growth_bad() {
  auto v = local_reserved();
  auto w = v;
  auto it = w.begin();
  w.reserve(w.capacity() + 1);
  return *it;
}
int erase_loop_ok() {
  auto v = local_reserved();
  for (auto it = v.begin(); it != v.end();) {
    it = v.erase(it);
  }
  return 0;
}

int copy_erase_keeps_source_ok() {
  auto v = local_reserved();
  auto it = v.begin() + 2;
  auto w = v;
  w.erase(w.begin());
  return *it;
}
int copy_destroy_keeps_source_ok() {
  auto v = local_reserved();
  auto it = v.begin() + 2;
  {
    auto w = v;
    w[0] = 4;
  }
  return *it;
}
int moved_iterator_live_ok() {
  auto v = local_reserved();
  auto it = v.begin() + 2;
  auto w = std::move(v);
  return *it;
}
int moved_iterator_clear_bad() {
  auto v = local_reserved();
  auto it = v.begin() + 2;
  auto w = std::move(v);
  w.clear();
  return *it;
}
int increment_keeps_copy_ok() {
  auto v = local_reserved();
  auto it = v.begin();
  auto first = it;
  ++it;
  v.erase(v.begin() + 1);
  return *first;
}
int increment_invalidated_bad() {
  auto v = local_reserved();
  auto it = v.begin();
  auto first = it;
  ++it;
  v.erase(v.begin() + 1);
  return *it;
}

} // namespace iterator_invalidation
