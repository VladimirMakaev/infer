/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <stdlib.h>

void deref_null_bad() {
  int* p = NULL;
  *p = 42;
}

// reported: the comment cannot be read when the source file is gone at report time
void deref_null_suppressed_but_unreadable_bad() {
  int* p = NULL;
  *p = 42; // @infer-ignore NULLPTR_DEREFERENCE
}
