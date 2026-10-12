/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

int unknown_int(void);

// A remainder by a literal is bounded by the divisor alone, so this holds even though
// nothing is known about the dividend.
void remainder_is_bounded_ok(void) {
  int r = unknown_int() % 3;
  if (r > 2 || r < -2) {
    int* p = 0;
    *p = 1;
  }
}

// A non-negative dividend gives a non-negative remainder.
void remainder_keeps_sign_ok(void) {
  int a = unknown_int();
  if (a < 0) {
    return;
  }
  int r = a % 7;
  if (r < 0) {
    int* p = 0;
    *p = 1;
  }
}

// Division truncates toward zero, so a positive dividend keeps a non-negative quotient.
void quotient_keeps_sign_ok(void) {
  int a = unknown_int();
  if (a <= 0) {
    return;
  }
  int q = a / 3;
  if (q < 0) {
    int* p = 0;
    *p = 1;
  }
}

// A negative literal divisor flips the quotient's sign.
void quotient_negative_divisor_ok(void) {
  int a = unknown_int();
  if (a <= 0) {
    return;
  }
  int q = a / -2;
  if (q > 0) {
    int* p = 0;
    *p = 1;
  }
}

// The bound must not be mistaken for an exact value: a remainder of 1 is perfectly possible.
void remainder_can_be_one_bad(void) {
  int r = unknown_int() % 3;
  if (r == 1) {
    int* p = 0;
    *p = 1;
  }
}

// A quotient that really can be negative must still be reported.
void quotient_can_be_negative_bad(void) {
  int a = unknown_int();
  if (a >= 0) {
    return;
  }
  int q = a / 3;
  if (q <= 0) {
    int* p = 0;
    *p = 1;
  }
}
