/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// The C string contract treats the terminating NUL as part of the searched string.
extern "C" char* strchr(const char*, int);
char* opaque_lookup(const char*, int);
int opaque_integer(int);

#ifndef CONTRACT_CASE
#define CONTRACT_CASE 0
#endif

namespace contract_routes {
#if CONTRACT_CASE == 0 || CONTRACT_CASE == 1
#ifndef CONTRACT_OMIT
int model_ok() {
  // @call lookup
#ifdef CONTRACT_BYPASS
  char* result = opaque_lookup("x", 0);
#else
  char* result = strchr("x", 0);
#endif
  // @operation read
  return *result;
}
#else
int unrelated_ok() { return 0; }
#endif
#endif

#if CONTRACT_CASE == 0 || CONTRACT_CASE == 2
int model_bad() {
  // @call absent_lookup
  char* result = strchr("x", 'z');
  // @operation absent_read
  return *result;
}
#endif

#if CONTRACT_CASE == 0 || CONTRACT_CASE == 3 || CONTRACT_CASE == 4
int read_pointer(int* pointer) { return *pointer; }
#endif

#if CONTRACT_CASE == 0 || CONTRACT_CASE == 3
int body_ok() {
  int value = 1;
  // @call safe_helper
  // @operation safe_helper
  return read_pointer(&value);
}
#endif

#if CONTRACT_CASE == 0 || CONTRACT_CASE == 4
int body_bad() {
  // @call null_helper
  // @operation null_helper
  return read_pointer(nullptr);
}
#endif

#if CONTRACT_CASE == 0 || CONTRACT_CASE == 5
int deliberate_unknown_ok() {
  // @call opaque
  // @operation opaque
  return opaque_integer(1);
}
#endif

#if CONTRACT_CASE == 0 || CONTRACT_CASE == 6
// A local array element read without initialization must be diagnosed.
int array_element_uninitialized_bad() {
  int values[2];
  // @operation uninitialized_read
  return values[1];
}
#endif
} // namespace contract_routes
