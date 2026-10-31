/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstdint>

// A union with a default member initializer is not an aggregate before C++14, so this file is
// skipped in the C++11 variant of this test directory.
#if __cplusplus >= 201402L

namespace union_init_list {

enum class Code : std::uint8_t { Ok = 0, Error = 1 };

struct Payload {
  int value = 7;
};

// A status whose code lives in a struct inside a union, written by an
// aggregate initializer and read back through the member name.
struct NestedUnionStatus {
  union Data {
    struct Components {
      Code code;
      std::uint8_t salt;
      std::uint16_t index;
    } components;
    std::uint32_t word = 0;
  };

  /* implicit */ constexpr NestedUnionStatus(Code code) noexcept
      : data_{.components = {.code = code, .salt = 0, .index = 0}} {}

  [[nodiscard]] constexpr bool is_not_ok() const noexcept {
    return Code::Ok != data_.components.code;
  }

 private:
  Data data_;
};

// Same shape, but the union member that gets initialized is a scalar.
struct ScalarUnionStatus {
  union Data {
    Code code;
    std::uint32_t word = 0;
  };

  /* implicit */ constexpr ScalarUnionStatus(Code code) noexcept
      : data_{.code = code} {}

  [[nodiscard]] constexpr bool is_not_ok() const noexcept {
    return Code::Ok != data_.code;
  }

 private:
  Data data_;
};

// Control: the same nesting without a union.
struct NestedStructStatus {
  struct Components {
    Code code;
    std::uint8_t salt;
    std::uint16_t index;
  };

  /* implicit */ constexpr NestedStructStatus(Code code) noexcept
      : data_{.code = code, .salt = 0, .index = 0} {}

  [[nodiscard]] constexpr bool is_not_ok() const noexcept {
    return Code::Ok != data_.code;
  }

 private:
  Components data_;
};

Payload* global_payload();

template <typename Status>
Status get(int key, Payload** out) {
  if (key > 0) {
    *out = global_payload();
    return Status(Code::Ok);
  }
  return Status(Code::Error);
}

// The out-parameter is only written on the success path, and the caller checks
// the status before dereferencing it.
int nested_union_checked_ok(int key) {
  Payload* payload = nullptr;
  if (get<NestedUnionStatus>(key, &payload).is_not_ok()) {
    return -1;
  }
  return payload->value;
}

int scalar_union_checked_ok(int key) {
  Payload* payload = nullptr;
  if (get<ScalarUnionStatus>(key, &payload).is_not_ok()) {
    return -1;
  }
  return payload->value;
}

int nested_struct_checked_ok(int key) {
  Payload* payload = nullptr;
  if (get<NestedStructStatus>(key, &payload).is_not_ok()) {
    return -1;
  }
  return payload->value;
}

// A status built from Code::Error really is not ok, so the branch that skips
// the check is reachable and the dereference is a true positive.
int nested_union_unchecked_bad() {
  Payload* payload = nullptr;
  NestedUnionStatus status = get<NestedUnionStatus>(0, &payload);
  return payload->value;
}

// The code written into the union must be readable back: with Code::Error the
// negated branch is unreachable, so this must not report.
int nested_union_error_is_not_ok_ok() {
  NestedUnionStatus status(Code::Error);
  if (!status.is_not_ok()) {
    Payload* payload = nullptr;
    return payload->value;
  }
  return 0;
}

} // namespace union_init_list

#endif // __cplusplus >= 201402L
