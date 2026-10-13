/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
#include <cstdarg>
#include <cstdio>
#include <string>

namespace variadic_return_param {

// Returns a string, so it is given a return parameter, which is appended after the
// arguments of `...` in the actuals.
std::string format(const char* fmt, ...) {
  char buffer[64];
  va_list args;
  va_start(args, fmt);
  std::vsnprintf(buffer, sizeof buffer, fmt, args);
  va_end(args);
  return std::string(buffer);
}

int deref(int* p) { return *p; }

// The zero is an int, not a pointer, and the return parameter is not the one it is
// passed to.
std::string zero_argument_ok() {
  const int port = 0;
  return format("%d", port);
}

std::string zero_argument_not_first_ok() {
  const int port = 0;
  return format("%s %d", "host", port);
}

std::string nonzero_argument_ok() {
  const int port = 7;
  return format("%d", port);
}

std::string unknown_argument_ok(int port) { return format("%d", port); }

std::string no_variadic_argument_ok() { return format("no arguments here"); }

std::string several_zero_arguments_ok() {
  const int a = 0;
  const int b = 0;
  return format("%d %d", a, b);
}

// a null pointer really is dereferenced, and nothing to do with the variadic call
int null_dereference_bad() {
  int* p = nullptr;
  return *p;
}

// a null pointer passed to a declared formal of a callee that dereferences it
int null_through_formal_bad() {
  int* p = nullptr;
  return deref(p);
}

} // namespace variadic_return_param
