/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
#include <string>

namespace string_fill_constructor {
int length_ok() {
  std::string s(5, 'a');
  if (s.size() != 5) {
    return *static_cast<int*>(nullptr);
  }
  return 0;
}
int length_bad() {
  std::string s(5, 'a');
  if (s.size() == 5) {
    return *static_cast<int*>(nullptr);
  }
  return 0;
}
int zero_ok() {
  std::string s(0, 'a');
  if (!s.empty() || s.size() != 0) {
    return *static_cast<int*>(nullptr);
  }
  return 0;
}
int embedded_zero_ok() {
  std::string s(5, '\0');
  if (s.length() != 5) {
    return *static_cast<int*>(nullptr);
  }
  return 0;
}
int copied_ok() {
  std::string s(5, 'a');
  std::string t(s);
  s.append("b"); // The copy is needed: the original is modified before reading t.
  if (t.size() != 5) {
    return *static_cast<int*>(nullptr);
  }
  return 0;
}
int mutated_bad() {
  std::string s(5, 'a');
  s.append("b");
  if (s.size() == 6) {
    return *static_cast<int*>(nullptr);
  }
  return 0;
}
} // namespace string_fill_constructor
