/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <stddef.h>

// this file and multi_main_b.c are the sources of two different programs
int main() {
  int* a = NULL;
  return *a;
}
