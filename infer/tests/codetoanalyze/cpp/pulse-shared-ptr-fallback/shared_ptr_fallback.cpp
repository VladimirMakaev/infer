/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "standin.h"

struct Payload { int value; };
using Owner = std::shared_ptr<Payload>;
typedef Owner OwnerAlias;

int copy_count_ok(Owner& held) {
  if (!held || held.use_count() != 1) return 0;
  std::vector<Owner> values;
  values.push_back(held);
  if (!held || held.use_count() != 2) return *static_cast<int*>(nullptr);
  return 0;
}

int move_count_ok(Owner& held) {
  if (!held || held.use_count() != 1) return 0;
  std::vector<Owner> values;
  values.push_back(std::move(held));
  if (held || held.use_count() != 0) return *static_cast<int*>(nullptr);
  return 0;
}

int const_lvalue_count_ok(const Owner& held) {
  if (!held || held.use_count() != 1) return 0;
  std::vector<Owner> values;
  values.emplace_back(held);
  if (!held || held.use_count() != 2) return *static_cast<int*>(nullptr);
  return 0;
}

int const_rvalue_count_ok(const Owner& held) {
  if (!held || held.use_count() != 1) return 0;
  std::vector<Owner> values;
  values.emplace_back(std::move(held));
  if (!held || held.use_count() != 2) return *static_cast<int*>(nullptr);
  return 0;
}

int alias_copy_count_ok(OwnerAlias& held) {
  if (!held || held.use_count() != 1) return 0;
  std::vector<Owner> values;
  values.push_back(held);
  if (!held || held.use_count() != 2) return *static_cast<int*>(nullptr);
  return 0;
}

int alias_move_count_ok(OwnerAlias& held) {
  if (!held || held.use_count() != 1) return 0;
  std::vector<Owner> values;
  values.push_back(std::move(held));
  if (held || held.use_count() != 0) return *static_cast<int*>(nullptr);
  return 0;
}

int const_alias_rvalue_count_ok(const OwnerAlias& held) {
  if (!held || held.use_count() != 1) return 0;
  std::vector<Owner> values;
  values.emplace_back(std::move(held));
  if (!held || held.use_count() != 2) return *static_cast<int*>(nullptr);
  return 0;
}

int const_pointee_move_count_ok(std::shared_ptr<const Payload>& held) {
  if (!held || held.use_count() != 1) return 0;
  std::vector<std::shared_ptr<const Payload>> values;
  values.emplace_back(std::move(held));
  if (held || held.use_count() != 0) return *static_cast<int*>(nullptr);
  return 0;
}

int copy_keeps_object_ok(std::vector<Owner>& values) {
  Owner held(new Payload{42});
  Payload* raw = held.get();
  values.push_back(held);
  held.reset();
  return raw->value;
}

int direct_drop_bad() {
  Owner held(new Payload{42});
  Payload* raw = held.get();
  held.reset();
  return raw->value;
}

int copy_count_bad(Owner& held) {
  if (!held || held.use_count() != 1) return 0;
  std::vector<Owner> values;
  values.push_back(held);
  if (held.use_count() == 2) return *static_cast<int*>(nullptr);
  return 0;
}

int moved_source_read_bad(Owner& held) {
  if (!held || held.use_count() != 1) return 0;
  std::vector<Owner> values;
  values.push_back(std::move(held));
  return (*held).value;
}
