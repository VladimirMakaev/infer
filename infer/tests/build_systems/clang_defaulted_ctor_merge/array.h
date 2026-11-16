/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

template <typename T> class Array {
 public:
  Array() = default;
  bool empty() const { return begin_ == end_; }

 private:
  T* begin_ = nullptr;
  T* end_ = nullptr;
};

struct Uninitialized {
  Uninitialized() = default;
  int get() const { return value; }
  int value;
  int other;
};

struct ExplicitEmpty {
  ExplicitEmpty() {}
  int get() const { return value; }
  int value;
  int other;
};

struct TrivialEmpty {
  TrivialEmpty() = default;
};
