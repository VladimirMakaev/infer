/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

namespace enum_switch {

enum class Code { A, B };
enum Fixed : unsigned char { F0, F1 };
// Values 1 and 3 are legal despite having no enumerator names.
enum Sparse { S0 = 0, S2 = 2 };

int scoped_implicit_ok(Code code) {
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

int scoped_explicit_ok(Code code) {
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

int fixed_implicit_ok(Fixed code) {
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

int fixed_explicit_ok(Fixed code) {
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

int sparse_implicit_ok(Sparse code) {
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

int sparse_explicit_ok(Sparse code) {
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

int integer_implicit_ok(int code) {
  int result = -1;
  switch (code) {
    case 0:
      result = 4;
      break;
    case 1:
      result = 5;
      break;
  }
  return result;
}

int scoped_overwrite_bad(Code code) {
  int result = 17;
  switch (code) {
    case Code::A:
      result = 4;
      break;
    case Code::B:
      result = 5;
      break;
    default:
      result = 6;
      break;
  }
  return result;
}

int sparse_overwrite_bad(Sparse code) {
  int result = 17;
  switch (code) {
    case S0:
      result = 4;
      break;
    case S2:
      result = 5;
      break;
    default:
      result = 6;
      break;
  }
  return result;
}

int overwrite_bad() {
  int result = 17;
  result = 23;
  return result;
}

// These enums exhaust their actual domains, including signed ranges.
enum Dense { D0, D1 };
enum SignedDense { N2 = -2, N1 = -1, N0 = 0, N3 = 1 };
// A zero-only enum can also hold 1.
enum ZeroOnly { Z0 = 0 };
enum BoolFixed : bool { B0 = false, B1 = true };
// Aliases increase the enumerator count without covering the holes.
enum AliasedSparse { A0 = 0, A0Alias = 0, A2 = 2, A2Alias = 2 };
enum NoZero { O1 = 1, O2 = 2, O3 = 3 };
enum SignedSparse { M2 = -2, P1 = 1 };
enum SignedFixed : signed char { CMinus = -1, CZero = 0 };

int dense_implicit_overwrite_bad(Dense code) {
  int result = 17;
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

int dense_explicit_overwrite_bad(Dense code) {
  int result = 17;
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

int signed_dense_overwrite_bad(SignedDense code) {
  int result = 17;
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

int zero_only_fallback_ok(ZeroOnly code) {
  int result = -1;
  switch (code) {
    case Z0:
      result = 4;
      break;
  }
  return result;
}

int bool_fixed_overwrite_bad(BoolFixed code) {
  int result = 17;
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

int aliased_sparse_fallback_ok(AliasedSparse code) {
  int result = -1;
  switch (code) {
    case A0:
      result = 4;
      break;
    case A2:
      result = 5;
      break;
  }
  return result;
}

int no_zero_fallback_ok(NoZero code) {
  int result = -1;
  switch (code) {
    case O1:
      result = 3;
      break;
    case O2:
      result = 4;
      break;
    case O3:
      result = 5;
      break;
  }
  return result;
}

int signed_sparse_fallback_ok(SignedSparse code) {
  int result = -1;
  switch (code) {
    case M2:
      result = 4;
      break;
    case P1:
      result = 5;
      break;
  }
  return result;
}

int signed_fixed_fallback_ok(SignedFixed code) {
  int result = -1;
  switch (code) {
    case CMinus:
      result = 4;
      break;
    case CZero:
      result = 5;
      break;
  }
  return result;
}


// The sign bit alone represents both -1 and 0.
enum SignedPair { PairMinus = -1, PairZero = 0 };

int signed_pair_overwrite_bad(SignedPair code) {
  int result = 17;
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


} // namespace enum_switch
