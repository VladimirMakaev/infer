/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#if __cplusplus >= 201703L

namespace aggregate_bases {

struct Left {
  int* left;
};
struct Right {
  int* right;
};
struct Pair : Left, Right {
  int value;
};
struct Empty {};
struct Triple : Empty, Left, Right {};
struct Extra {
  int* extra;
};
struct Nested : Pair, Extra {
  int value2;
};

int two_bases_ok() {
  int left = 1;
  int right = 2;
  Pair pair{{&left}, {&right}, 3};
  return *pair.left + *pair.right + pair.value;
}

int second_base_null_bad() {
  int left = 1;
  Pair pair{{&left}, {nullptr}, 3};
  return *pair.right;
}

int first_base_null_bad() {
  int right = 2;
  Pair pair{{nullptr}, {&right}, 3};
  return *pair.left;
}

int empty_base_ok() {
  int left = 1;
  int right = 2;
  Triple triple{{}, {&left}, {&right}};
  return *triple.left + *triple.right;
}

int empty_base_null_bad() {
  int left = 1;
  Triple triple{{}, {&left}, {nullptr}};
  return *triple.right;
}

int nested_bases_ok() {
  int left = 1;
  int right = 2;
  int extra = 3;
  Nested nested{{{&left}, {&right}, 4}, {&extra}, 5};
  return *nested.left + *nested.right + *nested.extra + nested.value +
         nested.value2;
}

int nested_base_null_bad() {
  int left = 1;
  int extra = 3;
  Nested nested{{{&left}, {nullptr}, 4}, {&extra}, 5};
  return *nested.right;
}

int conditional_base_ok(bool condition) {
  int left = 1;
  int right = 2;
  Pair pair{{condition ? &left : &right}, {&right}, 3};
  return *pair.left + *pair.right;
}

int implicit_base_zero_bad() {
  int left = 1;
  Pair pair{{&left}};
  return *pair.right;
}

int subsequent_null_bad() {
  int left = 1;
  int right = 2;
  Pair pair{{&left}, {&right}, 3};
  int* null_pointer = nullptr;
  return *null_pointer + pair.value;
}

} // namespace aggregate_bases

#endif
