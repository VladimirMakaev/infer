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

} // namespace enum_switch
