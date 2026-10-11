/* Copyright (c) Meta Platforms, Inc. and affiliates.
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree. */
#include <vector>
#include <utility>
namespace vector_size_model {
int assign_count_ok() {
  std::vector<int> v;
  v.push_back(7);
  v.assign(3, 9);
  if (v.size() != 3) {
    int* p = nullptr;
    return *p;
  }
  v.clear();
  return 0;
}
int assign_zero_ok() {
  std::vector<int> v;
  v.push_back(7);
  v.assign(0, 9);
  if (v.size() != 0) {
    int* p = nullptr;
    return *p;
  }
  v.clear();
  return 0;
}
int copy_assign_ok() {
  std::vector<int> a, b;
  a.push_back(7);
  b.push_back(1);
  b.push_back(2);
  a = b;
  if (a.size() != 2) {
    int* p = nullptr;
    return *p;
  }
  a.clear();
  return 0;
}
int copy_assign_empty_ok() {
  std::vector<int> a, b;
  a.push_back(7);
  a = b;
  if (a.size() != 0) {
    int* p = nullptr;
    return *p;
  }
  a.clear();
  return 0;
}
int copy_assign_size_bad() {
  std::vector<int> a, b;
  a.push_back(7);
  b.push_back(1);
  b.push_back(2);
  a = b;
  if (a.size() == 2) {
    int* p = nullptr;
    return *p;
  }
  a.clear();
  return 0;
}
int move_assign_ok() {
  std::vector<int> a, b;
  a.push_back(7);
  b.push_back(1);
  b.push_back(2);
  a = std::move(b);
  if (a.size() != 2 || b.size() != 0) {
    int* p = nullptr;
    return *p;
  }
  a.clear();
  b.clear();
  return 0;
}
int assign_list_ok() {
  std::vector<int> v;
  v.push_back(7);
  v.assign({1, 2});
  if (v.size() != 2) {
    int* p = nullptr;
    return *p;
  }
  v.clear();
  return 0;
}
int assign_list_operator_ok() {
  std::vector<int> v;
  v.push_back(7);
  v = {1, 2, 3};
  if (v.size() != 3) {
    int* p = nullptr;
    return *p;
  }
  v.clear();
  return 0;
}
// Existing conservative range-size gap.
int FP_assign_range_ok() {
  std::vector<int> v, src;
  src.push_back(1);
  src.push_back(2);
  v.assign(src.begin(), src.end());
  if (v.size() != 2) {
    int* p = nullptr;
    return *p;
  }
  v.clear();
  return 0;
}
int erase_one_ok() {
  std::vector<int> v;
  v.push_back(1);
  v.push_back(2);
  v.push_back(3);
  v.erase(v.begin());
  if (v.size() != 2) {
    int* p = nullptr;
    return *p;
  }
  v.clear();
  return 0;
}
// Existing conservative range-size gap.
int FP_erase_all_ok() {
  std::vector<int> v;
  v.push_back(1);
  v.push_back(2);
  v.erase(v.begin(), v.end());
  if (v.size() != 0) {
    int* p = nullptr;
    return *p;
  }
  v.clear();
  return 0;
}
// Existing conservative range-size gap.
int FP_erase_empty_range_ok() {
  std::vector<int> v;
  v.push_back(1);
  v.erase(v.begin(), v.begin());
  if (v.size() != 1) {
    int* p = nullptr;
    return *p;
  }
  v.clear();
  return 0;
}
int insert_one_ok() {
  std::vector<int> v;
  v.push_back(1);
  v.insert(v.end(), 2);
  if (v.size() != 2) {
    int* p = nullptr;
    return *p;
  }
  v.clear();
  return 0;
}
int insert_count_ok() {
  std::vector<int> v;
  v.push_back(1);
  v.insert(v.end(), 3, 2);
  if (v.size() != 4) {
    int* p = nullptr;
    return *p;
  }
  v.clear();
  return 0;
}
int insert_zero_ok() {
  std::vector<int> v;
  v.push_back(1);
  v.insert(v.end(), 0, 2);
  if (v.size() != 1) {
    int* p = nullptr;
    return *p;
  }
  v.clear();
  return 0;
}
int insert_list_ok() {
  std::vector<int> v;
  v.push_back(1);
  v.insert(v.end(), {2, 3});
  if (v.size() != 3) {
    int* p = nullptr;
    return *p;
  }
  v.clear();
  return 0;
}
// Existing conservative range-size gap.
int FP_insert_range_ok() {
  std::vector<int> v, src;
  v.push_back(1);
  src.push_back(2);
  src.push_back(3);
  v.insert(v.end(), src.begin(), src.end());
  if (v.size() != 3) {
    int* p = nullptr;
    return *p;
  }
  v.clear();
  return 0;
}
int resize_grow_ok() {
  std::vector<int> v;
  v.push_back(1);
  v.resize(3);
  if (v.size() != 3) {
    int* p = nullptr;
    return *p;
  }
  v.clear();
  return 0;
}
int resize_shrink_ok() {
  std::vector<int> v;
  v.push_back(1);
  v.push_back(2);
  v.resize(1);
  if (v.size() != 1) {
    int* p = nullptr;
    return *p;
  }
  v.clear();
  return 0;
}
int resize_zero_ok() {
  std::vector<int> v;
  v.push_back(1);
  v.resize(0);
  if (v.size() != 0) {
    int* p = nullptr;
    return *p;
  }
  v.clear();
  return 0;
}
int resized_size_bad() {
  std::vector<int> v;
  v.resize(3);
  if (v.size() == 3) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}
int copy_assignment_invalidates_bad() {
  std::vector<int> a, b;
  a.push_back(7);
  int* p = &a[0];
  a = b;
  a.clear();
  return *p;
}
int copy_assignment_keeps_source_ok() {
  std::vector<int> a, b;
  b.push_back(7);
  int* p = &b[0];
  a = b;
  a.clear();
  return *p;
}

int copy_untrusted_source_zero_guard_ok() {
  std::vector<int> v;
  v.push_back(7);
  std::vector<int> src{1, 2};
  v = src;
  if (v.size() == 0) {
    int* p = nullptr;
    return *p;
  }
  v.clear();
  return 0;
}
int insert_untrusted_receiver_size_bad() {
  std::vector<int> v{1, 2};
  v.insert(v.end(), 1, 3);
  if (v.size() == 3) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}
int insert_computed_count_size_bad() {
  std::vector<int> v;
  v.push_back(1);
  std::vector<int> other{2, 3};
  v.insert(v.end(), other.size(), 7);
  if (v.size() == 3) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}
} // namespace vector_size_model
