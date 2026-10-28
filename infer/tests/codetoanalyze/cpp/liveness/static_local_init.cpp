/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// A function-local static's initialiser is not translated, because it runs
// once and translating it at the declaration would claim it runs every time.
// The locals it reads are still read, so their stores are not dead.

struct D {
  int x;
};
const D& make();
int use(int);
int use2(int, int);
const D* build();

const D& static_lambda_init_ok() {
  const auto create = []() -> const D& { return make(); };
  static const D& my = create();
  return my;
}

int static_int_init_ok(int a) {
  const int k = a + 1;
  static const int s = use(k);
  return s;
}

int static_in_nested_block_ok(int a) {
  const int k = a + 1;
  if (a > 0) {
    static const int s = use(k);
    return s;
  }
  return 0;
}

int thread_local_init_ok(int a) {
  const int k = a + 1;
  thread_local const int s = use(k);
  return s;
}

int static_from_two_locals_ok(int a) {
  const int p = a + 1;
  const int q = a + 2;
  static const int s = use2(p, q);
  return s;
}

// the static's initialiser decides it one variable at a time: `used` is read
// by it and is not dead, `unused` is read by nothing and is
int static_from_other_local_bad(int a) {
  const int used = a + 1;
  int unused = a + 2;
  static const int s = use(used);
  return s;
}

// a static in the function does not keep an unrelated local alive
int dead_despite_static_bad() {
  int k = 1;
  static int s = 0;
  return s;
}

// the initialiser reads no local, so there is nothing to keep alive
int static_no_local_read_bad(int a) {
  int k = a + 1;
  static const int s = use(7);
  return s;
}

// the initialiser is an immediately invoked lambda whose locals belong to the
// lambda, not here: nothing in this frame is read, and nothing in this frame
// may be named by what the declaration emits
const D* iife_static_ok() {
  static const D* const cached = []() {
    D local{};
    int tmp = local.x + 1;
    return build();
  }();
  return cached;
}

// a local only captured by a lambda in the initialiser is still not seen: the
// capture list is not walked, only the expression around it
int FP_lambda_capture_in_static_init_ok(int a) {
  const int k = a + 1;
  static const int s = [k]() { return k; }();
  return s;
}

int ordinary_init_ok(int a) {
  const int k = a + 1;
  const int s = use(k);
  return s;
}

int static_constexpr_ok(int a) {
  constexpr int k = 41;
  static constexpr int s = k + 1;
  return s + a;
}

int real_dead_bad(int a) {
  int k = a + 1;
  k = 3;
  return use(0);
}
