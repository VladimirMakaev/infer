/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "array.h"
#include "missing.h"

struct Holder {
  Array<int> fields;
};

Holder make() { return Holder{}; }

int failed_defaulted_bad() {
  Uninitialized object;
  return object.get();
}

int failed_explicit_bad() {
  ExplicitEmpty object;
  return object.get();
}

void failed_trivial_ok() { TrivialEmpty object; }
