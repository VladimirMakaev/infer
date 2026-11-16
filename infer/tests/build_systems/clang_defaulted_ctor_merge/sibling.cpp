/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "array.h"

bool sibling_empty_ok() {
  Array<int> fields;
  return fields.empty();
}

int uninitialized_bad() {
  Uninitialized object;
  return object.get();
}

void trivial_ok() { TrivialEmpty object; }
