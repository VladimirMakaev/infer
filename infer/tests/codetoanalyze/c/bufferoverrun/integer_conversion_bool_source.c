/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

int choose_bool_source(void);

void bool_load_keeps_integer_source_bad(void) {
  int x = choose_bool_source() ? 2 : 2;
  _Bool value = (_Bool)x;
  if (x == 2 && value == 1) {
    int a[1] = {0};
    a[1] = 1;
  }
}

void bool_load_does_not_equal_integer_source_ok(void) {
  int x = choose_bool_source() ? -2 : -2;
  _Bool value = (_Bool)x;
  if (x == 1 && value == 1) {
    int a[1] = {0};
    a[1] = 1;
  }
}
