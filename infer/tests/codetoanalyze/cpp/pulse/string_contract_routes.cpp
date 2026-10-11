/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

extern "C" char* strchr(const char*, int);
extern "C" char* strrchr(const char*, int);

#ifndef CONTRACT_CASE
#define CONTRACT_CASE 0
#endif

namespace string_contract_routes {
#if CONTRACT_CASE == 0 || CONTRACT_CASE == 1
int strchr_zero_ok() {
  // @call strchr_zero_lookup
  char* result = strchr("x", 0);
  // @operation strchr_zero_read
  return *result;
}
#endif
#if CONTRACT_CASE == 0 || CONTRACT_CASE == 2
int strchr_absent_bad() {
  // @call strchr_absent_lookup
  char* result = strchr("x", 'z');
  // @operation strchr_absent_read
  return *result;
}
#endif
#if CONTRACT_CASE == 0 || CONTRACT_CASE == 3
int strchr_unknown_bad(int needle) {
  // @call strchr_unknown_lookup
  char* result = strchr("x", needle);
  // @operation strchr_unknown_read
  return *result;
}
#endif
#if CONTRACT_CASE == 0 || CONTRACT_CASE == 4
int strchr_null_bad() {
  // @call strchr_null_lookup
  // @operation strchr_null_read
  char* result = strchr(nullptr, 0);
  return result ? *result : 0;
}
#endif
#if CONTRACT_CASE == 0 || CONTRACT_CASE == 5
int strrchr_zero_ok() {
  // @call strrchr_zero_lookup
  char* result = strrchr("x", 0);
  // @operation strrchr_zero_read
  return *result;
}
#endif
#if CONTRACT_CASE == 0 || CONTRACT_CASE == 6
int strrchr_absent_bad() {
  // @call strrchr_absent_lookup
  char* result = strrchr("x", 'z');
  // @operation strrchr_absent_read
  return *result;
}
#endif
#if CONTRACT_CASE == 0 || CONTRACT_CASE == 7
int strrchr_unknown_bad(int needle) {
  // @call strrchr_unknown_lookup
  char* result = strrchr("x", needle);
  // @operation strrchr_unknown_read
  return *result;
}
#endif
#if CONTRACT_CASE == 0 || CONTRACT_CASE == 8
int strrchr_null_bad() {
  // @call strrchr_null_lookup
  // @operation strrchr_null_read
  char* result = strrchr(nullptr, 0);
  return result ? *result : 0;
}
#endif
} // namespace string_contract_routes
