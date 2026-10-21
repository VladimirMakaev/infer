/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

namespace empty_class_temporary {

struct Identity {
  template <class T>
  T&& operator()(T&& t) const {
    return static_cast<T&&>(t);
  }
};

struct Deref {
  int operator()(int* p) const { return *p; }
};

template <class Projection>
int project(int* p, Projection projection = {}) {
  return projection(*p);
}

template <class Function>
int run(int* p, Function f) {
  return f(p);
}

int default_argument_good() {
  int x = 7;
  return project<Identity>(&x);
}

int brace_temporary_good() {
  int x = 7;
  return run(&x, Deref{});
}

int default_argument_null_bad() {
  int* p = nullptr;
  return project<Identity>(p);
}

int brace_temporary_null_bad() {
  int* p = nullptr;
  return run(p, Deref{});
}

} // namespace empty_class_temporary
