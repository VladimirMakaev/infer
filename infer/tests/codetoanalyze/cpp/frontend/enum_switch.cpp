/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

enum class Code { A, B };
enum Fixed : unsigned char { F0, F1 };
// Values 1 and 3 are legal despite having no enumerator names.
enum Sparse { S0 = 0, S2 = 2 };

int scoped_implicit(Code code) {
  int result = -1;
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

int scoped_explicit(Code code) {
  int result = -1;
  switch (code) {
    case Code::A:
      result = 4;
      break;
    case Code::B:
      result = 5;
      break;
    default:
      break;
  }
  return result;
}

int fixed_implicit(Fixed code) {
  int result = -1;
  switch (code) {
    case F0:
      result = 4;
      break;
    case F1:
      result = 5;
      break;
  }
  return result;
}

int fixed_explicit(Fixed code) {
  int result = -1;
  switch (code) {
    default:
      break;
    case F0:
      result = 4;
      break;
    case F1:
      result = 5;
      break;
  }
  return result;
}

int sparse_implicit(Sparse code) {
  int result = -1;
  switch (code) {
    case S0:
      result = 4;
      break;
    case S2:
      result = 5;
      break;
  }
  return result;
}

int sparse_explicit(Sparse code) {
  int result = -1;
  switch (code) {
    case S0:
      result = 4;
      break;
    default:
      break;
    case S2:
      result = 5;
      break;
  }
  return result;
}

int scoped_default_fallthrough(Code code) {
  int result = -1;
  switch (code) {
    case Code::A:
      result = 4;
      break;
    default:
      result = 6;
    case Code::B:
      result += 1;
      break;
  }
  return result;
}

// Named values exhaust the entire non-fixed enum domain in these cases.
enum Dense { D0, D1 };
enum SignedDense { N2 = -2, N1 = -1, N0 = 0, N3 = 1 };
// A zero-only enum can also hold 1, so its fallback remains reachable.
enum ZeroOnly { Z0 = 0 };
enum BoolFixed : bool { B0 = false, B1 = true };

int dense_implicit(Dense code) {
  int result = -1;
  switch (code) {
    case D0:
      result = 4;
      break;
    case D1:
      result = 5;
      break;
  }
  return result;
}

int dense_explicit(Dense code) {
  int result = -1;
  switch (code) {
    case D0:
      result = 4;
      break;
    case D1:
      result = 5;
      break;
    default:
      break;
  }
  return result;
}

int signed_dense_implicit(SignedDense code) {
  int result = -1;
  switch (code) {
    case N2:
      result = 2;
      break;
    case N1:
      result = 3;
      break;
    case N0:
      result = 4;
      break;
    case N3:
      result = 5;
      break;
  }
  return result;
}

int zero_only_implicit(ZeroOnly code) {
  int result = -1;
  switch (code) {
    case Z0:
      result = 4;
      break;
  }
  return result;
}

int bool_fixed_implicit(BoolFixed code) {
  int result = -1;
  switch (code) {
    case B0:
      result = 4;
      break;
    case B1:
      result = 5;
      break;
  }
  return result;
}

// The sign bit alone represents both -1 and 0.
enum SignedPair { PairMinus = -1, PairZero = 0 };

int signed_pair_implicit(SignedPair code) {
  int result = -1;
  switch (code) {
    case PairMinus:
      result = 4;
      break;
    case PairZero:
      result = 5;
      break;
  }
  return result;
}
