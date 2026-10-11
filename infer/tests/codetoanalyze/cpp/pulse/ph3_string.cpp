/* Copyright (c) Meta Platforms, Inc. and affiliates.
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree. */
#include <string>
namespace ph3_string_regressions {
int literal_size_ok() {
  std::string s("abcd");
  if (s.size() != 4) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}
int default_size_ok() {
  std::string s;
  if (s.size() != 0) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}
int copy_size_ok() {
  std::string s("abcd");
  std::string t(s);
  s.clear();
  if (t.size() != 4) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}
int literal_size_bad() {
  std::string s("abcd");
  if (s.size() == 4) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}
int unknown_size_bad(std::string& s) {
  if (s.size() == 4) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}
int mutation_keeps_unknown_bad() {
  std::string s("abcd");
  s.append("ef");
  if (s.size() == 6) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}
} // namespace ph3_string_regressions
