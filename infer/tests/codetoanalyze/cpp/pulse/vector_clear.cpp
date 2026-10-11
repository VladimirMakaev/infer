/* Copyright (c) Facebook, Inc. and its affiliates.
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree. */
#include <vector>

namespace vector_clear_model {
int clear_size_ok() {
  std::vector<int> v;
  v.push_back(7);
  v.clear();
  if (v.size() != 0 || !v.empty()) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}

void clear_in_helper(std::vector<int>& v) { v.clear(); }

int clear_size_helper_ok() {
  std::vector<int> v;
  v.push_back(7);
  clear_in_helper(v);
  if (v.size() != 0) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}

int clear_alias_bad() {
  std::vector<int> v;
  v.push_back(7);
  int* p = &v[0];
  v.clear();
  return *p;
}

int clear_alias_helper_bad() {
  std::vector<int> v;
  v.push_back(7);
  int* p = &v[0];
  clear_in_helper(v);
  return *p;
}
} // namespace vector_clear_model
