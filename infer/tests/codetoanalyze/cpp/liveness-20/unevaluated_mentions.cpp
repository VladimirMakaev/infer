/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Reporting controls for a store whose only mention is UNEVALUATED, in a
// template instantiation that also discards an `if constexpr` branch.
//
// In an instantiation clang empties the discarded arm, so a read that lived
// only there leaves nothing behind. The declaration's referenced-but-not-used
// pair of bits is the only remaining sign of it -- and the same pair describes
// a local named only in `decltype`, which is a dead store and must be
// reported. The two are not distinguishable here: `decltype` keeps no
// expression in the exported AST, and the template pattern, where the read
// does survive, is not exported either. So the bits cannot be acted on, and
// every case below except the first must keep reporting.
//
// The first case is the false positive that costs: it is live in the <true>
// instantiation. Silencing it is only worth doing if the rest stay reported.

namespace unevaluated_mentions {

void consume(int);

// FALSE POSITIVE: the read of `value` is in the arm this instantiation
// discarded, so the store is live in `live_in_one_ok<true>`.
template <bool B>
int FP_live_in_one_ok(int* p) {
  int value = *p;
  if constexpr (B) {
    consume(value);
  }
  return 0;
}

// `never_read` is named only by decltype, which evaluates nothing: the
// initializer's value is never used in any instantiation.
template <bool B>
int type_only_bad(int* p) {
  int never_read = *p;
  decltype(never_read) result = *p + 1;
  if constexpr (B) {
    consume(0);
  }
  return result;
}

// the discarded arm is nested inside another, and still unrelated to the local
template <bool B>
int nested_type_only_bad(int* p) {
  int never_read = *p;
  decltype(never_read) result = *p + 1;
  if constexpr (B) {
    if constexpr (!B) {
      consume(0);
    }
  }
  return result;
}

// noexcept is unevaluated too, but unlike decltype it is an expression, so the
// mention does survive instantiation
template <bool B>
bool noexcept_only_bad(int* p) {
  int never_read = *p;
  if constexpr (B) {
    consume(0);
  }
  return noexcept(never_read);
}

// the operator() of a generic lambda is a member function template
struct disabled {
  static constexpr bool enabled = false;
};

int generic_type_only_bad(int* p) {
  auto function = [p](auto tag) {
    int never_read = *p;
    decltype(never_read) result = *p + 1;
    if constexpr (decltype(tag)::enabled) {
      consume(0);
    }
    return result;
  };
  return function(disabled{});
}

// an ordinary member of a class template
template <bool B>
struct holder {
  int member_type_only_bad(int* p) {
    int never_read = *p;
    decltype(never_read) result = *p + 1;
    if constexpr (B) {
      consume(0);
    }
    return result;
  }
};

// dead in every instantiation, and named nowhere at all
template <bool B>
int dead_in_all_bad(int* p) {
  int value = *p;
  if constexpr (B) {
    consume(0);
  } else {
    consume(1);
  }
  return 0;
}

// dead on its own terms: overwritten before the only read
template <bool B>
int overwritten_bad(int* p) {
  int value = *p;
  value = 1;
  if constexpr (B) {
    consume(0);
  }
  return value;
}

// the same shape outside a template, where the discarded branch is translated
// and the existing handling applies
int non_template_bad(int* p) {
  int value = *p;
  if (false) {
    consume(0);
  }
  return 0;
}

// KNOWN, upstream policy: a mention in an unevaluated operand counts as a use,
// so a sizeof-only local is silent. Recorded here to keep the sizeof case and
// the decltype case visibly apart -- they are not treated alike.
template <bool B>
int size_only_ok(int* p) {
  int never_read = *p;
  if constexpr (B) {
    consume(0);
  }
  return sizeof(never_read);
}

// Attests which standard this directory compiles at: exactly one of these is
// translated, so the expected output names it and a change to the Makefile's
// -std is visible as a diff rather than passing quietly. `if constexpr` is
// C++17, and the reports above are the same under -std=c++17; only C++20 is
// gated here, because a sibling directory for C++17 would need its own copy
// of this file.
#if __cplusplus == 201703L
int cxx17_arm_attestation_bad(int* p) {
  int value = *p;
  return 0;
}
#else
int cxx20_arm_attestation_bad(int* p) {
  int value = *p;
  return 0;
}
#endif

int instantiate(int* p) {
  FP_live_in_one_ok<true>(p);
  FP_live_in_one_ok<false>(p);
  dead_in_all_bad<true>(p);
  dead_in_all_bad<false>(p);
  overwritten_bad<false>(p);
  size_only_ok<false>(p);
  return type_only_bad<false>(p) + nested_type_only_bad<false>(p) +
         (noexcept_only_bad<false>(p) ? 1 : 0) + generic_type_only_bad(p) +
         holder<false>{}.member_type_only_bad(p);
}

} // namespace unevaluated_mentions
