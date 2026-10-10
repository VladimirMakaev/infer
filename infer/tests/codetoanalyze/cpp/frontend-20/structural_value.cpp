/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

template <double Value>
struct DoubleValue {};

template <auto... Values>
struct ValuePack {};

template <typename T, int Integral, auto Value, auto... Rest>
struct Mixed {};

DoubleValue<3.14>* float_capture_ok(DoubleValue<3.14>* value) { return value; }

DoubleValue<0x1.0000000000001p0>* adjacent_value_ok(
    DoubleValue<0x1.0000000000001p0>* value) {
  return value;
}

ValuePack<1.0f, 1.0, 2.0f, 0.0, -0.0>* multiple_types_ok(
    ValuePack<1.0f, 1.0, 2.0f, 0.0, -0.0>* value) {
  return value;
}

Mixed<DoubleValue<3.14>, -7, 1.0f, 1.0, 0x1.0000000000001p0>* nested_values_ok(
    Mixed<DoubleValue<3.14>, -7, 1.0f, 1.0, 0x1.0000000000001p0>* value) {
  return value;
}

Mixed<int, 3, 4, nullptr>* integral_type_control_ok(
    Mixed<int, 3, 4, nullptr>* value) {
  return value;
}

template <auto Value>
int function_template_ok() {
  return 7;
}

int call_function_template_ok() { return function_template_ok<3.14>(); }

template <auto Value>
inline int variable_template = 7;

int variable_template_ok() { return variable_template<3.14>; }

struct Data {
  int array[3];
};
inline Data data{};

ValuePack<&data.array[0]>* first_element_ok(ValuePack<&data.array[0]>* value) {
  return value;
}

ValuePack<&data.array[1]>* second_element_ok(ValuePack<&data.array[1]>* value) {
  return value;
}

struct Base {
  int field;
};
struct Derived : Base {};

ValuePack<static_cast<int Derived::*>(&Base::field)>* member_pointer_ok(
    ValuePack<static_cast<int Derived::*>(&Base::field)>* value) {
  return value;
}
