/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

namespace maybe_unused_parameters {

void consume(unsigned short);

void annotated_postincrement_ok([[maybe_unused]] unsigned short id) {
  consume(id++);
  consume(id++);
}

void gnu_unused_postincrement_ok(unsigned short id __attribute__((unused))) {
  consume(id++);
  consume(id++);
}

void annotated_assignment_ok([[maybe_unused]] int id) { id = 42; }

void plain_postincrement_bad(unsigned short id) {
  consume(id++);
  consume(id++);
}

void plain_assignment_bad(int id) { id = 42; }

void annotated_second_parameter_ok(int first, [[maybe_unused]] int second) {
  consume(first);
  second = 42;
}

void mixed_parameters_bad([[maybe_unused]] int first, int second) {
  first = 42;
  second = 42;
}

[[maybe_unused]] void function_attribute_parameter_bad(int id) { id = 42; }

void shadowed_local_bad([[maybe_unused]] int id) {
  id = 42;
  { int id = 7; }
}

struct Parameters {
  void annotated_member_ok([[maybe_unused]] unsigned short id) {
    consume(id++);
    consume(id++);
  }

  void plain_member_bad(unsigned short id) {
    consume(id++);
    consume(id++);
  }

  void mixed_member_bad(int ordinary, [[maybe_unused]] int ignored) {
    ordinary = 42;
    ignored = 42;
  }

  static void annotated_static_member_ok([[maybe_unused]] int id) { id = 42; }
};

void annotated_lambda_ok() {
  auto increment = []([[maybe_unused]] unsigned short id) {
    consume(id++);
    consume(id++);
  };
  increment(1);
}

template <class T>
void annotated_template_ok([[maybe_unused]] T id) {
  id = 42;
}

void instantiate_annotated_template_ok() { annotated_template_ok(0); }

} // namespace maybe_unused_parameters
