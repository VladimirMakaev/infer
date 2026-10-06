/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstdlib>
#include <limits>
#include <string>
#include <type_traits>

// the headers of the standard library are not translated
namespace library_constants {

std::string kind_of(const char* raw) {
  std::string name(raw);
  if (name.find('.') == std::string::npos) {
    return "plain";
  }
  return "";
}

int npos_compare_in_callee_then_null_deref_bad(const char* raw) {
  std::string kind = kind_of(raw);
  if (kind.empty()) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}

int npos_value_ok() {
  if (std::string::npos != (std::string::size_type)-1) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}

int numeric_limits_signed_digits_ok() {
  if (std::numeric_limits<int>::digits != 31) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}

int numeric_limits_unsigned_digits_ok() {
  if (std::numeric_limits<unsigned char>::digits != 8) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}

template <typename T>
bool is_signed_type() {
  return std::numeric_limits<T>::is_signed;
}

int numeric_limits_unsigned_is_signed_bad() {
  if (!is_signed_type<unsigned int>()) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}

int numeric_limits_signed_is_signed_ok() {
  if (!is_signed_type<int>()) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}

template <typename Int>
unsigned long to_unsigned(Int value) {
  if (!(std::is_unsigned<Int>::value || value >= 0)) {
    std::abort();
  }
  return static_cast<unsigned long>(value);
}

struct parse_context {
  const char* begin;
  unsigned long size;
  int next_id;
};

template <typename T>
const char* parse_spec(parse_context& ctx) {
  return ctx.begin;
}

template <typename... Args>
struct spec_checker {
  using parse_func = const char* (*)(parse_context&);
  static constexpr int num_args = sizeof...(Args);
  parse_context ctx;
  parse_func parse_funcs[num_args > 0 ? num_args : 1];

  spec_checker(const char* s, unsigned long n)
      : ctx{s, n, 0}, parse_funcs{&parse_spec<Args>...} {}

  const char* on_spec(int id, const char* it) {
    ctx.size -= to_unsigned(it - ctx.begin);
    ctx.begin = it;
    return id >= 0 && id < num_args ? parse_funcs[id](ctx) : it;
  }
};

const char* is_unsigned_then_empty_pack_table_ok(const char* s,
                                                 unsigned long n) {
  spec_checker<> checker(s, n);
  int id = checker.ctx.next_id++;
  return checker.on_spec(id, s);
}

} // namespace library_constants
