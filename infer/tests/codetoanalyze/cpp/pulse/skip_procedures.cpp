/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// skipped by --pulse-skip-procedures in the Makefile
namespace skip_procedures {

struct Skipped {
  Skipped() {
    int* p = nullptr;
    *p = 42;
  }

  int get_ok() const {
    int* p = nullptr;
    return *p;
  }
};

int skipped_function_ok() {
  int* p = nullptr;
  return *p;
}

} // namespace skip_procedures

int call_skipped_procedures_ok() {
  skip_procedures::Skipped s;
  return s.get_ok() + skip_procedures::skipped_function_ok();
}

int not_skipped_bad() {
  int* p = nullptr;
  return *p;
}
