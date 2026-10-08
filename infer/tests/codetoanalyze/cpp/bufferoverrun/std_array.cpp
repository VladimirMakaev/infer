/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
#include <array>

int std_array_bo_Bad() {
  std::array<int, 42> a;
  return a[42];
}

int normal_array_bo() {
  int b[42];
  return b[42];
}

void new_char_Good() {
  uint64_t len = 13;
  char* dst;
  dst = new char[len];
}

void new_int1_Bad() {
  uint64_t len = 4611686018427387903; // (1 << 62) - 1
  int32_t* dst;
  dst = new int32_t[len];
}

void new_int2_Bad() {
  uint64_t len = 9223372036854775807; // (1 << 63) - 1
  int32_t* dst;
  dst = new int32_t[len];
}

void new_int3_Bad() {
  uint64_t len = 18446744073709551615; // (1 << 64) - 1
  int32_t* dst;
  dst = new int32_t[len];
}

void std_array_contents_Good() {
  std::array<int, 10> a;
  a[0] = 5;
  a[a[0]] = 0;
}

void std_array_contents_Bad() {
  std::array<int, 10> a;
  a[0] = 10;
  a[a[0]] = 0;
}

void array_iter1_Good() {
  std::array<int, 11> a;
  for (auto it = a.begin(); it < a.end(); ++it) {
    *it = 10;
  }
  a[a[0]] = 0;
}

void array_iter1_Bad() {
  std::array<int, 5> a;
  for (auto it = a.begin(); it < a.end(); ++it) {
    *it = 10;
  }
  a[a[0]] = 0;
}

void array_iter2_Good() {
  std::array<int, 11> a;
  for (auto it = a.begin(); it != a.end(); ++it) {
    *it = 10;
  }
  a[a[0]] = 0;
}

void array_iter2_Bad() {
  std::array<int, 5> a;
  for (auto it = a.begin(); it != a.end(); ++it) {
    *it = 10;
  }
  a[a[0]] = 0;
}

void array_iter3_Good() {
  std::array<int, 11> a = {10};
  for (auto it = a.cbegin(); it < a.cend(); ++it) {
    a[*it] = 10;
  }
}

void array_iter3_Bad() {
  std::array<int, 5> a = {10};
  for (auto it = a.cbegin(); it < a.cend(); ++it) {
    a[*it] = 10;
  }
}

void array_iter_front_Good() {
  std::array<int, 11> a;
  a.front() = 10;
  a[a[0]] = 0;
}

void array_iter_front_Bad() {
  std::array<int, 5> a;
  a.front() = 10;
  a[a[0]] = 0;
}

void array_iter_back_Good() {
  std::array<int, 11> a;
  a.back() = 10;
  a[a[0]] = 0;
}

void array_iter_back_Bad() {
  std::array<int, 5> a;
  a.back() = 10;
  a[a[0]] = 0;
}

void array_rev_iter_Good() {
  std::array<int, 11> a;
  for (auto it = a.rbegin(); it < a.rend(); ++it) {
    *it = 10;
  }
  a[a[0]] = 0;
}

void array_rev_iter_Bad_FN() {
  std::array<int, 5> a;
  for (auto it = a.rbegin(); it < a.rend(); ++it) {
    *it = 10;
  }
  a[a[0]] = 0;
}

void malloc_zero_Bad() { int* a = (int*)malloc(sizeof(int) * 0); }

void new_array_zero_Good() { int* a = new int[0]; }

std::array<int, 4> global_std_array;

int read_global_std_array_Bad() { return global_std_array[4]; }

int read_global_std_array_Good() { return global_std_array[3]; }

std::array<std::array<int, 3>, 2> global_nested_std_array;

int read_global_nested_std_array_Good() {
  return global_nested_std_array[1][2];
}

std::array<unsigned, 4> global_std_array_counts;

static unsigned minus_one(unsigned n) { return n - 1u; }

static unsigned minus_one_global_std_array(int i) {
  return minus_one(global_std_array_counts[i & 3]);
}

// reported in each caller, as for other global arrays
unsigned call_minus_one_global_std_array1_Bad(int i) {
  return minus_one_global_std_array(i);
}

unsigned call_minus_one_global_std_array2_Bad(int i) {
  return minus_one_global_std_array(i + 1);
}

struct std_array_member {
  std::array<int, 5> a;
};

void std_array_member_local_Good() {
  std_array_member x;
  x.a[4] = 0;
}

void std_array_member_local_Bad() {
  std_array_member x;
  x.a[5] = 0;
}

void std_array_member_iter_Good() {
  std_array_member x;
  for (auto it = x.a.begin(); it < x.a.end(); ++it) {
    *it = 4;
  }
  x.a[x.a[0]] = 0;
}

void std_array_member_iter_Bad() {
  std_array_member x;
  for (auto it = x.a.begin(); it < x.a.end(); ++it) {
    *it = 5;
  }
  x.a[x.a[0]] = 0;
}

struct std_array_2d_member {
  std::array<std::array<int, 3>, 2> a;
};

void std_array_2d_member_local_Good() {
  std_array_2d_member x;
  x.a[1][2] = 0;
}

void std_array_2d_member_local_inner_Bad() {
  std_array_2d_member x;
  x.a[1][3] = 0;
}

void std_array_2d_member_local_outer_Bad() {
  std_array_2d_member x;
  x.a[2][0] = 0;
}

int std_array_2d_member_loop_Good(std_array_2d_member& x) {
  for (int i = 0; i < 2; i++) {
    x.a[i][2] = 1;
  }
  return x.a[1][2];
}

int std_array_2d_member_param_Bad(std_array_2d_member& x) { return x.a[0][3]; }

int std_array_2d_row_ref_Good(std_array_2d_member& x) {
  std::array<int, 3>& row = x.a[1];
  return row.front() + row.back() + row.at(2);
}

int std_array_2d_row_ref_Bad(std_array_2d_member& x) {
  std::array<int, 3>& row = x.a[1];
  return row[3];
}

int std_array_of_c_arrays_Good() {
  std::array<int[3], 2> a = {};
  return a[1][2];
}

int std_array_of_c_arrays_Bad() {
  std::array<int[3], 2> a = {};
  return a[1][3];
}

int std_array_3d_Good() {
  std::array<std::array<std::array<int, 3>, 2>, 4> a = {};
  return a[3][1][2];
}

int std_array_3d_Bad() {
  std::array<std::array<std::array<int, 3>, 2>, 4> a = {};
  return a[3][1][3];
}

// As for a std::array passed by reference, the inner arrays are not found.
int FN_std_array_2d_ref_param_Bad(std::array<std::array<int, 3>, 2>& a) {
  return a[0][3];
}

struct std_array_ref_member {
  std::array<int, 4>& a;
};

// As for a std::array passed by reference, the models of std::array methods do
// not find the array of a std::array reference member.
void FN_std_array_ref_member_end_Bad(std_array_ref_member& x) {
  *x.a.end() = 0;
}
