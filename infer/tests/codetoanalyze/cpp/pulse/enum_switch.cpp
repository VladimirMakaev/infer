/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

namespace enum_switch {

enum class Code { A, B };
enum Sparse { S0 = 0, S2 = 2 };

int scoped_implicit_fallback_bad() {
  Code code = static_cast<Code>(3);
  switch (code) {
    case Code::A:
      return 4;
    case Code::B:
      return 5;
  }
  int* ptr = nullptr;
  return *ptr;
}

int scoped_explicit_fallback_bad() {
  Code code = static_cast<Code>(3);
  switch (code) {
    case Code::A:
      return 4;
    case Code::B:
      return 5;
    default: {
      int* ptr = nullptr;
      return *ptr;
    }
  }
}

int sparse_explicit_fallback_bad() {
  Sparse code = static_cast<Sparse>(1);
  switch (code) {
    case S0:
      return 4;
    default: {
      int* ptr = nullptr;
      return *ptr;
    }
    case S2:
      return 5;
  }
}

int scoped_named_case_ok() {
  Code code = Code::A;
  switch (code) {
    case Code::A:
      return 4;
    case Code::B:
      return 5;
  }
  int* ptr = nullptr;
  return *ptr;
}

int sparse_named_case_ok() {
  Sparse code = S2;
  switch (code) {
    case S0:
      return 4;
    default: {
      int* ptr = nullptr;
      return *ptr;
    }
    case S2:
      return 5;
  }
}

// The scrutinee below is never shown to be outside the enumeration, so the
// fallback edge of a switch that names every enumerator must not be taken.

int unknown_scrutinee_local_ok(Code code) {
  int result;
  switch (code) {
    case Code::A:
      result = 4;
      break;
    case Code::B:
      result = 5;
      break;
  }
  return result;
}

struct Pair {
  int first;
  int second;
};

int unknown_scrutinee_struct_local_ok(Code code) {
  Pair pair;
  switch (code) {
    case Code::A:
      pair = Pair{1, 2};
      break;
    case Code::B:
      pair = Pair{3, 4};
      break;
  }
  return pair.first;
}

int* make_pointer();

int unknown_scrutinee_null_local_ok(Code code) {
  int* ptr = nullptr;
  switch (code) {
    case Code::A:
      ptr = make_pointer();
      break;
    case Code::B:
      ptr = make_pointer();
      break;
  }
  return *ptr;
}

Pair pair_of(Code code) {
  switch (code) {
    case Code::A:
      return Pair{1, 2};
    case Code::B:
      return Pair{3, 4};
  }
}

int unknown_scrutinee_fall_off_the_end_ok(Code code) {
  return pair_of(code).first;
}

enum class Width : unsigned char { A, B };

int unknown_scrutinee_fixed_width_ok(Width width) {
  int result;
  switch (width) {
    case Width::A:
      result = 4;
      break;
    case Width::B:
      result = 5;
      break;
  }
  return result;
}

// An explicit default is the program's own edge, so it keeps being analysed
// whatever the scrutinee is.
int unknown_scrutinee_explicit_default_bad(Code code) {
  switch (code) {
    case Code::A:
      return 4;
    case Code::B:
      return 5;
    default: {
      int* ptr = nullptr;
      return *ptr;
    }
  }
}

enum class Triple { A, B, C };

// A case that assigns nothing leaves the end of the switch uninitialized.
int empty_case_bad(Triple triple) {
  int result;
  switch (triple) {
    case Triple::A:
      result = 4;
      break;
    case Triple::B:
      result = 5;
      break;
    case Triple::C:
      break;
  }
  return result;
}

// A switch that names only some of the enumerators keeps its ordinary fallback.
int unhandled_enumerator_bad(Triple triple) {
  int result;
  switch (triple) {
    case Triple::A:
      result = 4;
      break;
    case Triple::B:
      result = 5;
      break;
  }
  return result;
}

} // namespace enum_switch
