/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// __builtin_bit_cast applied to an lvalue is clang's LValueToRValueBitCast: read the operand's
// object representation and reinterpret it as the target type. The reinterpretation is below
// SIL's level of detail, but the READ is not optional -- without it the operand's ADDRESS
// stands in for its value, and returning it is reported as a stack address escaping.
//
// This is not an exotic corner: arm_neon.h builds the result of every NEON intrinsic with
// __builtin_bit_cast over a materialised temporary, so on an aarch64 target it made ordinary
// functions returning a scalar or a SIMD value by value report. The cases here are the
// portable reduction of that -- no NEON, no aarch64, so they run in the normal x86 suite.
//
// no <cstdint> and no std:: here: this file is also compiled with -std=c++11 by the pulse-11
// case, and __builtin_bit_cast is a builtin available in every language mode.

namespace builtin_bit_cast {

unsigned long long src();
double dsrc();

struct Pair {
  unsigned a, b;
};
Pair psrc();

// the operand is a named local
double bit_cast_local_ok() {
  unsigned long long x = src();
  return __builtin_bit_cast(double, x);
}

// the operand is a materialised temporary: the shape arm_neon.h produces
double bit_cast_temporary_ok() { return __builtin_bit_cast(double, src()); }

// the operand is bound to a const reference
double bit_cast_ref_to_temporary_ok() {
  const unsigned long long& r = src();
  return __builtin_bit_cast(double, r);
}

// struct in, scalar out. The dereference exemption for structs is on the type PRODUCED, not
// the one consumed, so this still has to load: a struct address standing in for a double is
// exactly the bug.
double bit_cast_struct_to_scalar_ok() { return __builtin_bit_cast(double, psrc()); }

// scalar in, struct out: SIL carries a struct as its address, so no load here
Pair bit_cast_scalar_to_struct_ok() { return __builtin_bit_cast(Pair, src()); }

// chained, the way the intrinsics chain them
double bit_cast_chained_ok() {
  return __builtin_bit_cast(double,
                            __builtin_bit_cast(unsigned long long, dsrc()));
}

// ------------------------------- true positive controls -------------------------------

int* address_of_local_bad() {
  int x = 3;
  return &x;
}

const double* address_of_local_double_bad() {
  double d = 1.0;
  return &d;
}

// A SCALAR return type that really does carry a stack address out. This is the control
// against "fixing" the family above by suppressing the report whenever the return type is a
// scalar -- that rule looks right and would silence this real bug.
long address_of_local_as_integer_bad() {
  int x = 3;
  return (long)&x;
}

long address_of_temporary_as_integer_bad() {
  return (long)&(const int&)42;
}

} // namespace builtin_bit_cast
