/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <type_traits>

namespace structural_value {

template <auto... Args>
struct Values {};

template <typename T, int N, auto Value, auto... Rest>
struct Mixed {};

// These checks exercise Infer's std::is_same_v model, including its comparison
// of the template arguments in the captured types.
void equal_value_bad() {
  if (std::is_same_v<Values<3.14>, Values<3.14>>)
    *static_cast<int*>(nullptr) = 1;
}

void adjacent_double_ok() {
  if (std::is_same_v<Values<1.0>, Values<0x1.0000000000001p0>>)
    *static_cast<int*>(nullptr) = 1;
}

void adjacent_long_double_ok() {
#if __LDBL_MANT_DIG__ > 60
  if (std::is_same_v<Values<1.0L>, Values<0x1.000000000000001p0L>>)
    *static_cast<int*>(nullptr) = 1;
#endif
}

void signed_zero_ok() {
  if (std::is_same_v<Values<0.0>, Values<-0.0>>)
    *static_cast<int*>(nullptr) = 1;
}

void distinct_nan_payloads_ok() {
  if (std::is_same_v<Values<__builtin_nan("0x1")>,
                     Values<__builtin_nan("0x2")>>)
    *static_cast<int*>(nullptr) = 1;
}

void equal_nan_payload_bad() {
  if (std::is_same_v<Values<__builtin_nan("0x1")>,
                     Values<__builtin_nan("0x1")>>)
    *static_cast<int*>(nullptr) = 1;
}

void float_double_ok() {
  if (std::is_same_v<Values<1.0f>, Values<1.0>>)
    *static_cast<int*>(nullptr) = 1;
}

void double_long_double_ok() {
  if (std::is_same_v<Values<1.0>, Values<1.0L>>)
    *static_cast<int*>(nullptr) = 1;
}

void integral_float_ok() {
  if (std::is_same_v<Values<1>, Values<1.0>>)
    *static_cast<int*>(nullptr) = 1;
}

void different_pack_order_ok() {
  if (std::is_same_v<Values<1.0, 2.0>, Values<2.0, 1.0>>)
    *static_cast<int*>(nullptr) = 1;
}

void different_nested_value_ok() {
  if (std::is_same_v<Mixed<Values<1.0>, -7, 3.14, 1.0f>,
                     Mixed<Values<2.0>, -7, 3.14, 1.0f>>)
    *static_cast<int*>(nullptr) = 1;
}

void equal_nested_value_bad() {
  if (std::is_same_v<Mixed<Values<1.0>, -7, 3.14, 1.0f>,
                     Mixed<Values<1.0>, -7, 3.14, 1.0f>>)
    *static_cast<int*>(nullptr) = 1;
}

void integral_type_control_bad() {
  if (std::is_same_v<Mixed<int, 7, 3>, Mixed<int, 7, 3>>)
    *static_cast<int*>(nullptr) = 1;
}

void integral_type_control_ok() {
  if (std::is_same_v<Mixed<int, 7, 3>, Mixed<long, 7, 3>>)
    *static_cast<int*>(nullptr) = 1;
}

struct Data {
  int array[3];
};
inline Data data{};

void distinct_lvalue_paths_ok() {
  if (std::is_same_v<Values<&data.array[0]>, Values<&data.array[1]>>)
    *static_cast<int*>(nullptr) = 1;
}

void equal_lvalue_path_bad() {
  if (std::is_same_v<Values<&data.array[1]>, Values<&data.array[1]>>)
    *static_cast<int*>(nullptr) = 1;
}

inline int backing;

template <double Value>
int* choose_pointer(int* p) {
  if constexpr (Value == 1.0)
    return p;
  else
    return nullptr;
}

void function_specializations_ok() { *choose_pointer<1.0>(&backing) = 1; }

void function_specializations_bad() {
  *choose_pointer<0x1.0000000000001p0>(&backing) = 1;
}

template <double Value>
inline int* pointer;

void variable_specializations_ok() {
  pointer<1.0> = &backing;
  pointer<0x1.0000000000001p0> = nullptr;
  *pointer<1.0> = 1;
}

void variable_specializations_bad() {
  pointer<1.0> = nullptr;
  pointer<0x1.0000000000001p0> = &backing;
  *pointer<1.0> = 1;
}

} // namespace structural_value
