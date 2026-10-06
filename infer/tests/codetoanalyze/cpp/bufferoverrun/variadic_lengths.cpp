/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstdarg>
#include <cstring>

namespace variadic_lengths {

// Without a length bound or overflow check, the accumulated size can wrap.
char* concat_unbounded_bad(const char* first, ...) {
  if (!first)
    return nullptr;
  va_list args;
  va_start(args, first);
  std::size_t length = std::strlen(first);
  const char* part;
  while ((part = va_arg(args, const char*)) != nullptr)
    length += std::strlen(part);
  va_end(args);

  char* result = new char[length + 1];
  char* cursor = result;
  *cursor = '\0';
  va_start(args, first);
  std::strcpy(cursor, first);
  cursor += std::strlen(first);
  while ((part = va_arg(args, const char*)) != nullptr) {
    std::strcpy(cursor, part);
    cursor += std::strlen(part);
  }
  va_end(args);
  return result;
}

void bounded_copy_ok(const char* text, ...) {
  va_list args;
  va_start(args, text);
  char result[16];
  if (std::strlen(text) < sizeof(result))
    std::strcpy(result, text);
  va_end(args);
}

void bounded_copy_call_ok() {
  bounded_copy_ok("abcd", static_cast<const char*>(nullptr));
}

void small_copy_bad(const char* text, ...) {
  va_list args;
  va_start(args, text);
  char result[2];
  std::strcpy(result, text);
  va_end(args);
}

void small_copy_call_bad() {
  small_copy_bad("abcd", static_cast<const char*>(nullptr));
}

} // namespace variadic_lengths
