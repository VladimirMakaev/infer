/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

namespace negative_sentinel_default {

int next_int();
unsigned next_unsigned();
float next_float();

int minus_one_bad() {
  int value = -1;
  value = next_int();
  return value;
}

int minus_two_bad() {
  int value = -2;
  value = next_int();
  return value;
}

int negative_zero_ok() {
  int value = -0;
  value = next_int();
  return value;
}

int positive_bad() {
  int value = 17;
  value = next_int();
  return value;
}

int dynamic_negative_bad(int input) {
  int value = -input;
  value = next_int();
  return value;
}

unsigned unsigned_negative_bad() {
  unsigned value = -1U;
  value = next_unsigned();
  return value;
}

float floating_negative_bad() {
  float value = -1.0f;
  value = next_float();
  return value;
}

float nan_bad() {
  float value = __builtin_nanf("");
  value = next_float();
  return value;
}

} // namespace negative_sentinel_default
