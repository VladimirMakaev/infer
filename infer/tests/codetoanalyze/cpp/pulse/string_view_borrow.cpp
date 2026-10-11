/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
#include <string>
#include <string_view>

#if __cplusplus >= 201703L
namespace string_view_borrow {
char temporary_bad() {
  std::string_view v = std::string("abcde");
  return v[0];
}
char deleted_owner_bad() {
  auto* s = new std::string("abcde");
  std::string_view v(*s);
  delete s;
  return v[0];
}
char growth_bad() {
  std::string s("abcde");
  std::string_view v(s);
  s.append(1000, 'x');
  return v[0];
}
char nonzero_index_bad() {
  auto* s = new std::string("abcde");
  std::string_view v(*s);
  delete s;
  return v[2];
}
char copy_bad() {
  auto* s = new std::string("abcde");
  std::string_view first(*s);
  auto second = first;
  delete s;
  return second[0];
}
char substr_bad() {
  auto* s = new std::string("abcde");
  auto v = std::string_view(*s).substr(1, 2);
  delete s;
  return v[0];
}
char live_ok() {
  std::string s("abcde");
  return std::string_view(s)[0];
}
char copied_owner_ok() {
  std::string s("abcde");
  std::string t(s);
  std::string_view v(t);
  s.append(1000, 'x');
  return v[0];
}
char literal_ok() { return std::string_view("abcde")[2]; }
char live_substr_ok() {
  std::string s("abcde");
  return std::string_view(s).substr(1, 2)[0];
}
size_t expired_size_ok() {
  std::string_view v = std::string("abcde");
  return v.size(); // size() does not access the borrowed buffer.
}
char substr_data_bad() {
  auto* s = new std::string("abcde");
  auto v = std::string_view(*s).substr(1);
  auto* p = v.data();
  delete s;
  return p[0];
}
int pointer_identity_ok() {
  std::string s("abcde");
  std::string_view v(s);
  if (&v[0] != v.data() || &v[2] != &v.data()[2]) {
    return *static_cast<int*>(nullptr);
  }
  return 0;
}
int reverse_pointer_identity_ok() {
  std::string s("abcde");
  std::string_view v(s);
  const char* p = &v.data()[2];
  if (p != &v[2]) {
    return *static_cast<int*>(nullptr);
  }
  return 0;
}
} // namespace string_view_borrow
#endif
