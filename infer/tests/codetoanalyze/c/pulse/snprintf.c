/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <stdarg.h>
#include <stdio.h>

int snprintf_null_zero_ok(void) { return snprintf(NULL, 0, "%d", 42); }

int vsnprintf_null_zero_ok(const char* format, ...) {
  va_list args;
  va_start(args, format);
  int result = vsnprintf(NULL, 0, format, args);
  va_end(args);
  return result;
}

int snprintf_null_positive_bad(void) { return snprintf(NULL, 4, "%d", 42); }

int vsnprintf_null_positive_bad(const char* format, ...) {
  va_list args;
  va_start(args, format);
  int result = vsnprintf(NULL, 4, format, args);
  va_end(args);
  return result;
}

void snprintf_null_guarded_zero_ok(size_t size) {
  if (size == 0) {
    snprintf(NULL, size, "%d", 42);
  }
}

void vsnprintf_null_guarded_zero_ok(size_t size, const char* format, ...) {
  if (size == 0) {
    va_list args;
    va_start(args, format);
    vsnprintf(NULL, size, format, args);
    va_end(args);
  }
}

void snprintf_null_guarded_positive_bad(size_t size) {
  if (size > 0) {
    snprintf(NULL, size, "%d", 42);
  }
}

void vsnprintf_null_guarded_positive_bad(size_t size, const char* format, ...) {
  if (size > 0) {
    va_list args;
    va_start(args, format);
    vsnprintf(NULL, size, format, args);
    va_end(args);
  }
}

void snprintf_zero_preserves_buffer_ok(void) {
  char buffer[1] = {1};
  snprintf(buffer, 0, "%d", 42);
  if (buffer[0] != 1) {
    int* p = NULL;
    *p = 0;
  }
}

int snprintf_nonzero_buffer_ok(void) {
  char buffer[4];
  return snprintf(buffer, sizeof(buffer), "%d", 42);
}

int vsnprintf_nonzero_buffer_ok(const char* format, ...) {
  char buffer[4];
  va_list args;
  va_start(args, format);
  int result = vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  return result;
}

int snprintf_zero_null_format_bad(void) { return snprintf(NULL, 0, NULL); }

void snprintf_zero_then_null_deref_bad(void) {
  snprintf(NULL, 0, "%d", 42);
  int* p = NULL;
  *p = 0;
}

void vsnprintf_zero_then_null_deref_bad(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vsnprintf(NULL, 0, format, args);
  va_end(args);
  int* p = NULL;
  *p = 0;
}
