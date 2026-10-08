/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
#include <cstddef>
#include <initializer_list>

namespace initializer_list {

// a buffer whose storage is null when it is empty
struct IntBuffer {
  int* begin_ = nullptr;
  int* end_ = nullptr;

  IntBuffer(std::initializer_list<int> list) {
    std::size_t n = list.size();
    begin_ = n ? new int[n] : nullptr;
    end_ = begin_;
    for (const int* p = list.begin(); p != list.end(); ++p) {
      *end_++ = *p;
    }
  }

  ~IntBuffer() { delete[] begin_; }
};

int read_first_of_list_ok() {
  IntBuffer buffer{1, 2, 3};
  return *buffer.begin_;
}

std::size_t size_of(std::initializer_list<int> list) { return list.size(); }

int size_of_list_bad() {
  if (size_of({1, 2, 3}) == 3) {
    int* q = nullptr;
    return *q;
  }
  return 0;
}

int size_of_list_ok() {
  if (size_of({1, 2, 3}) != 3) {
    int* q = nullptr;
    return *q;
  }
  return 0;
}

int sum(std::initializer_list<int> list) {
  int s = 0;
  for (int x : list) {
    s += x;
  }
  return s;
}

int sum_of_list_ok() { return sum({1, 2}); }

int begin_end_of_local_list_ok() {
  std::initializer_list<int> list = {1, 2, 3};
  if (list.end() - list.begin() != static_cast<long>(list.size())) {
    int* q = nullptr;
    return *q;
  }
  return 0;
}

// the length of a list held in a local variable is unknown
int FP_size_of_local_list_ok() {
  std::initializer_list<int> list = {1, 2, 3};
  if (list.size() != 3) {
    int* q = nullptr;
    return *q;
  }
  return 0;
}

} // namespace initializer_list
