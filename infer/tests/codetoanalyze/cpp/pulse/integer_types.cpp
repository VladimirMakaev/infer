/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

namespace integer_types {

void char16_non_ascii_bad() {
  char16_t c = u'\u4e2d';
  if (c == 0x4e2d) {
    int* p = nullptr;
    *p = 42;
  }
}

void char32_emoji_bad() {
  char32_t c = U'\U0001F600';
  if (c == 0x1F600) {
    int* p = nullptr;
    *p = 42;
  }
}

void wchar_non_ascii_bad() {
  wchar_t c = L'\u00e9';
  if (c == 0xe9) {
    int* p = nullptr;
    *p = 42;
  }
}

void int128_large_bad() {
  __int128 x = 4000000000LL;
  if (x > 3000000000LL) {
    int* p = nullptr;
    *p = 42;
  }
}

void uint128_large_bad() {
  unsigned __int128 x = 5000000000ULL;
  if (x > 4294967295ULL) {
    int* p = nullptr;
    *p = 42;
  }
}

char16_t unknown_char16();

void char16_nonnegative_ok() {
  char16_t c = unknown_char16();
  if (c < 0) {
    int* p = nullptr;
    *p = 42;
  }
}

char32_t unknown_char32();

void char32_nonnegative_ok() {
  char32_t c = unknown_char32();
  if (c < 0) {
    int* p = nullptr;
    *p = 42;
  }
}

} // namespace integer_types
