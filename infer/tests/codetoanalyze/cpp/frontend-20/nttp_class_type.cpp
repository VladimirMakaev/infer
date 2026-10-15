/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// A class-type non-type template parameter is read as a global whose name comes
// from its value, so parameters that differ only by value stay distinct.
struct Tag {
  int v;
  int w;
};

static constexpr Tag kA{7, 3};
static constexpr Tag kB{9, 4};

template <Tag K>
int sum() {
  return K.v + K.w;
}

int use_a() { return sum<kA>(); }
int use_b() { return sum<kB>(); }

template <Tag K>
struct Traceable {
  int x;
  int get() const { return K.v; }
};

int use_member() {
  Traceable<kA> t{1};
  return t.get() + t.x;
}

// an integral parameter, long supported, must keep working
template <int N>
int plain() {
  return N + 1;
}

int use_plain() { return plain<5>(); }
