/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
#include <string>
#include <string_view>

#if __cplusplus >= 201703L
namespace string_view_accessors {
int literal_ok() {
  std::string_view v("abcde");
  if (v.size() != 5 || v.length() != 5 || v.empty()) {
    return *static_cast<int*>(nullptr);
  }
  return 0;
}
int literal_bad() {
  std::string_view v("abcde");
  if (v.size() == 5) {
    return *static_cast<int*>(nullptr);
  }
  return 0;
}
int explicit_count_ok() {
  std::string_view v("abcde", 3);
  if (v.size() != 3 || v.empty()) {
    return *static_cast<int*>(nullptr);
  }
  return 0;
}
int default_ok() {
  std::string_view v;
  if (!v.empty() || v.size() != 0) {
    return *static_cast<int*>(nullptr);
  }
  return 0;
}
int conversion_ok() {
  std::string s("abcde");
  std::string_view v(s);
  if (v.size() != 5) {
    return *static_cast<int*>(nullptr);
  }
  return 0;
}
int fill_conversion_ok() {
  std::string s(5, 'a');
  std::string_view v(s);
  if (v.size() != 5) {
    return *static_cast<int*>(nullptr);
  }
  return 0;
}
int copy_ok() {
  std::string_view first("abcde");
  std::string_view second(first);
  if (second.size() != 5) {
    return *static_cast<int*>(nullptr);
  }
  return 0;
}
int assign_ok() {
  std::string_view v;
  v = "abcde";
  if (v.size() != 5) {
    return *static_cast<int*>(nullptr);
  }
  return 0;
}
int substr_ok() {
  std::string_view v("abcde");
  if (v.substr(0, 2).size() != 2 || v.substr(3, 9).size() != 2 ||
      !v.substr(5).empty()) {
    return *static_cast<int*>(nullptr);
  }
  return 0;
}
int unknown_bad(std::string_view v) {
  if (v.size() == 5) {
    return *static_cast<int*>(nullptr);
  }
  return 0;
}
} // namespace string_view_accessors
#endif
