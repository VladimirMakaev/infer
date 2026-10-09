/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

namespace negative_sentinel_configured {

int next_int();
unsigned next_unsigned();
float next_float();
bool next_bool();

int minus_one_ok() {
  int value = -1;
  value = next_int();
  return value;
}

int nested_minus_one_ok() {
  int value = -(-(-1));
  value = next_int();
  return value;
}

int positive_ok() {
  int value = 7;
  value = next_int();
  return value;
}

int nested_positive_ok() {
  int value = -(-7);
  value = next_int();
  return value;
}

int cast_minus_one_ok() {
  int value = static_cast<int>(-1);
  value = next_int();
  return value;
}

int minus_two_bad() {
  int value = -2;
  value = next_int();
  return value;
}

int configured_operand_bad() {
  int value = -7;
  value = next_int();
  return value;
}

int double_negative_bad() {
  int value = -(-1);
  value = next_int();
  return value;
}

unsigned unsigned_negative_bad() {
  unsigned value = -1U;
  value = next_unsigned();
  return value;
}

unsigned unsigned_conversion_bad() {
  unsigned value = -1;
  value = next_unsigned();
  return value;
}

unsigned wrapped_positive_ok() {
  unsigned value = -4294967289U;
  value = next_unsigned();
  return value;
}

bool boolean_cast_bad() {
  bool value = static_cast<bool>(-1);
  value = next_bool();
  return value;
}

int dynamic_negative_bad(int input) {
  int value = -input;
  value = next_int();
  return value;
}

int positive_bad() {
  int value = 17;
  value = next_int();
  return value;
}

float floating_negative_bad() {
  float value = -1.0f;
  value = next_float();
  return value;
}

float floating_conversion_bad() {
  float value = -1;
  value = next_float();
  return value;
}

float nan_bad() {
  float value = __builtin_nanf("");
  value = next_float();
  return value;
}

} // namespace negative_sentinel_configured
