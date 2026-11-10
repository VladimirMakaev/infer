/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace const_field_copy {
struct Record {
  std::string first;
  std::string second;
  std::string third;
};
struct Single {
  std::string value;
};
using Key = std::tuple<std::string, std::string, std::string>;
using Counts = std::map<Key, int>;

struct Accumulator {
  Counts counts;
  // The result owns its strings. Map keys are const objects, so moving from
  // these bindings still copies; borrowing fields would change the result's
  // lifetime. Neither suggestion removes a copy while preserving ownership.
  std::vector<Record> map_keys_ok() {
    std::vector<Record> result;
    result.reserve(counts.size());
    for (const auto& [key, count] : counts) {
      const auto& [first, second, third] = key;
      result.push_back({first, second, third});
    }
    return result;
  }
};

Record borrowed_const_key_ok(const Key& key) {
  const auto& [first, second, third] = key;
  return {first, second, third};
}

Record owned_strings_bad(std::string first,
                         std::string second,
                         std::string third) {
  return {first, second, third};
}

// Top-level const on an owned value can be removed before moving its last use.
Record const_owned_strings_bad(const std::string first,
                               const std::string second,
                               const std::string third) {
  return {first, second, third};
}

Record moved_strings_ok(std::string first,
                        std::string second,
                        std::string third) {
  return {std::move(first), std::move(second), std::move(third)};
}

Single borrowed_const_reference_ok(const std::string& value) { return {value}; }

Single mutable_alias_bad(std::string value) {
  auto& alias = value;
  return {alias};
}

// The field copy is already missed when a const view hides an owned value.
Single FN_const_view_of_owned_bad(std::string value) {
  const auto& alias = value;
  return {alias};
}

std::string make_string();
// The field copy of this const local is also an existing missed report.
Single FN_const_owned_local_bad() {
  const auto value = make_string();
  return {value};
}

void consume(std::string value);
void read(const std::string& value);
// A callee parameter can take const-reference even when an owned field cannot.
void const_intermediate_bad(const std::string value) { consume(value); }

void used_after_ok(std::string value) {
  consume(value);
  read(value);
}
} // namespace const_field_copy
