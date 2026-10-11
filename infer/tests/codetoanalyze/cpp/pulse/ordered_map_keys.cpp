/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
#include <map>
#include <set>

namespace ordered_map_keys {
int erased_reference_bad() {
  std::map<int, int> m;
  m[1] = 11;
  m[2] = 22;
  int& r = m[1];
  m.erase(1);
  return r;
}
int missing_find_bad() {
  std::map<int, int> m;
  m[1] = 11;
  return m.find(999)->second;
}
int insert_reference_ok() {
  std::map<int, int> m;
  m[1] = 11;
  int& r = m[1];
  m[2] = 22;
  return r;
}
int other_erase_ok() {
  std::map<int, int> m;
  m[1] = 11;
  m[2] = 22;
  int& r = m[1];
  m.erase(2);
  return r;
}
int find_present_ok() {
  std::map<int, int> m;
  m[1] = 11;
  auto it = m.find(1);
  m[2] = 22;
  return it->second;
}
int missing_guard_ok() {
  std::map<int, int> m;
  m[1] = 11;
  auto it = m.find(999);
  return it == m.end() ? 0 : it->second;
}
int single_size_ok() {
  std::map<int, int> m;
  m[7] = 1;
  if (m.size() != 1 || m.empty()) {
    return *static_cast<int*>(nullptr);
  }
  return 0;
}
int single_size_bad() {
  std::map<int, int> m;
  m[7] = 1;
  if (m.size() == 1) {
    return *static_cast<int*>(nullptr);
  }
  return 0;
}
int repeated_size_ok() {
  std::map<int, int> m;
  m[7] = 1;
  m[7] = 2;
  if (m.size() != 1) {
    return *static_cast<int*>(nullptr);
  }
  return 0;
}
int two_keys_ok() {
  std::map<int, int> m;
  m[7] = 1;
  m[8] = 2;
  if (m.size() != 2) {
    return *static_cast<int*>(nullptr);
  }
  return 0;
}
int erase_size_ok() {
  std::map<int, int> m;
  m[7] = 1;
  m.erase(7);
  if (!m.empty() || m.size() != 0) {
    return *static_cast<int*>(nullptr);
  }
  return 0;
}
int absent_erase_size_ok() {
  std::map<int, int> m;
  m[7] = 1;
  m.erase(8);
  if (m.size() != 1) {
    return *static_cast<int*>(nullptr);
  }
  return 0;
}
int erased_find_bad() {
  std::map<int, int> m;
  m[7] = 1;
  m.erase(7);
  return m.find(7)->second;
}
int reinsert_ok() {
  std::map<int, int> m;
  m[7] = 1;
  m.erase(7);
  m[7] = 2;
  return m[7];
}
int old_reference_after_reinsert_bad() {
  std::map<int, int> m;
  int& r = m[7];
  m.erase(7);
  m[7] = 2;
  return r;
}
int erase_iterator_then_reinsert_ok() {
  std::map<int, int> m;
  m[7] = 1;
  m.erase(m.find(7));
  m[7] = 2;
  return m[7];
}
int set_insert_ok() {
  std::set<int> s{1, 2};
  auto it = s.find(1);
  s.insert(3);
  return *it;
}
int converting_comparator_bad() {
  std::map<int, int, std::less<bool>> m;
  m[1] = 11;
  m[2] = 22;
  if (m.size() == 1) { // Both keys compare equivalent after conversion to bool.
    return *static_cast<int*>(nullptr);
  }
  return 0;
}
int reference_insert_then_erase_bad() {
  std::map<int, int> m;
  m[1] = 11;
  auto it = m.find(1);
  m.insert({2, 22});
  m.erase(1);
  return it->second;
}
int reference_insert_then_other_erase_ok() {
  std::map<int, int> m;
  m[1] = 11;
  m[2] = 22;
  auto it = m.find(1);
  m.insert({3, 33});
  m.erase(2);
  return it->second;
}
int clear_after_insert_bad() {
  std::map<int, int> m;
  m[1] = 11;
  int& r = m[1];
  m.insert({2, 22});
  m.clear();
  return r;
}
int erased_last_iterator_bad() {
  std::map<int, int> m;
  m[1] = 11;
  return m.erase(m.find(1))->second;
}
int parameter_find_erase_bad(std::map<int, int>& m) {
  auto it = m.find(1);
  if (it == m.end()) {
    return 0;
  }
  m.erase(1);
  return it->second;
}
int unsupported_insert_reinsertion_ok() {
  std::map<int, int> m;
  m[1] = 11;
  m.erase(1);
  m.insert({1, 22});
  return m.find(1)->second;
}
int input_find_miss_insert_ok(std::map<int, int>& m) {
  auto it = m.find(1);
  if (it == m.end() && m[1] != 0) {
    return *static_cast<int*>(nullptr);
  }
  return 0;
}
int present_find_ok() {
  std::map<int, int> m;
  m[1] = 11;
  if (m.find(1) == m.end()) {
    return *static_cast<int*>(nullptr);
  }
  return 0;
}
int input_find_miss_then_insert_ok(std::map<int, int>& m) {
  auto it = m.find(1);
  if (it == m.end()) {
    m.insert({1, 22});
    return m.find(1)->second;
  }
  return 0;
}
int saved_find_then_subscript_ok(std::map<int, int>& m) {
  auto it = m.find(1);
  m[1] = 7;
  if (it == m.end()) {
    return m.find(1)->second;
  }
  return 0;
}
int parameter_clear_bad(std::map<int, int>& m) {
  const int& value = m[1];
  m.clear();
  return value;
}
int unknown_key_update_bad(int key) {
  std::map<int, int> m;
  m[1] = 0;
  m[key] = 7;
  if (m[1] != 0) {
    return *static_cast<int*>(nullptr);
  }
  return 0;
}
int different_key_update_ok(int key) {
  if (key == 1) {
    return 0;
  }
  std::map<int, int> m;
  m[1] = 0;
  m[key] = 7;
  if (m[1] != 0) {
    return *static_cast<int*>(nullptr);
  }
  return 0;
}
// Pre-existing precision gap: an unknown-key lookup still loses this zero-value fact.
// Keep the diagnostic explicit; this is not one of the constant-key batch regressions.
int FP_unknown_key_read_only_ok(int key) {
  std::map<int, int> m;
  m[1] = 0;
  const int& value = m[key];
  (void)value;
  if (m[1] != 0) {
    return *static_cast<int*>(nullptr);
  }
  return 0;
}
void mutate(std::map<int, int>&);
int unknown_mutation_bad() {
  std::map<int, int> m;
  m[7] = 1;
  mutate(m);
  if (m.size() != 1) {
    return *static_cast<int*>(nullptr);
  }
  return 0;
}
} // namespace ordered_map_keys
