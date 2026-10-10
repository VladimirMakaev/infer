/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstring>

int cpp_strchr_zero_ok(const char* text) { return *std::strchr(text, 0); }

int cpp_strchr_empty_ok() { return *std::strchr("", '\0'); }

int cpp_strchr_buffer_ok() {
  char text[] = "abc";
  *std::strchr(text, 0) = 0;
  return text[0];
}

int cpp_strchr_guarded_zero_ok(int needle) {
  if (needle == 0)
    return *std::strchr("abc", needle);
  return 0;
}

int cpp_strrchr_zero_ok() { return *std::strrchr("abc", 0); }

int cpp_strchr_null_bad() {
  const char* text = nullptr;
  return *std::strchr(text, 0);
}

int cpp_strrchr_null_bad() {
  const char* text = nullptr;
  return *std::strrchr(text, 0);
}

int cpp_strchr_missing_bad() { return *std::strchr("abc", 'z'); }

int cpp_strrchr_missing_bad() { return *std::strrchr("abc", 'z'); }

int cpp_strchr_unknown_bad(int needle) { return *std::strchr("abc", needle); }

int cpp_strrchr_unknown_bad(int needle) { return *std::strrchr("abc", needle); }

int cpp_memchr_bounded_zero_bad() {
  return *static_cast<const char*>(std::memchr("abc", 0, 2));
}

int cpp_memchr_empty_range_bad() {
  return *static_cast<const char*>(std::memchr("abc", 0, 0));
}
