/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <algorithm>
#include <cstring>

namespace pointer_alias {
void consume(int*);
void consume_indirect(int**);
void unrelated();
int plain_ok() {
  int current = 5;
  int* alias = &current;
  current = 7;
  return *alias;
}

unsigned char cast_ok() {
  int current = 5;
  auto* alias = reinterpret_cast<unsigned char*>(&current);
  current = 7;
  return *alias;
}

void unused_alias_bad() {
  int current = 5;
  int* alias = &current;
  current = 7;
  (void)alias;
}

int reassigned_alias_bad() {
  int current = 5;
  int other = 9;
  int* alias = &current;
  current = 7;
  alias = &other;
  return *alias;
}

int copied_alias_ok() {
  int current = 5;
  int* alias = &current;
  int* copy = alias;
  current = 7;
  return *copy;
}

int reference_ok() {
  int current = 5;
  int& alias = current;
  current = 7;
  return alias;
}

void copy_n_ok(int* output) {
  int current = 5;
  int* alias = &current;
  current = 7;
  std::copy_n(alias, 1, output);
}

void memcpy_ok(int* output) {
  int current = 5;
  int* alias = &current;
  current = 7;
  std::memcpy(output, alias, sizeof(current));
}

int overwritten_through_alias_bad() {
  int current = 5;
  int* alias = &current;
  current = 7;
  *alias = 9;
  return *alias;
}

int branch_ok(bool choose) {
  int current = 5;
  int other = 9;
  int* alias;
  if (choose)
    alias = &current;
  else
    alias = &other;
  current = 7;
  return *alias;
}

int double_pointer_ok() {
  int current = 5;
  int* alias = &current;
  int** indirect = &alias;
  current = 7;
  return **indirect;
}

int indirect_reassignment_ok() {
  int current = 5;
  int other = 9;
  int* alias = &other;
  int** indirect = &alias;
  *indirect = &current;
  current = 7;
  return *alias;
}

int copy_survives_reassignment_ok() {
  int current = 5;
  int other = 9;
  int* alias = &current;
  int* copy = alias;
  alias = &other;
  current = 7;
  return *copy + *alias;
}

int branch_keeps_unknown_ok(int* external, bool choose) {
  int current = 5;
  int* alias = external;
  if (choose)
    alias = &current;
  current = 7;
  *alias = 9;
  return current;
}

int partial_write_ok() {
  int current = 5;
  auto* alias = reinterpret_cast<unsigned char*>(&current);
  current = 7;
  *alias = 9;
  return current;
}

int loop_ok(unsigned count) {
  int current = 5;
  int* alias = &current;
  while (count--)
    current = 7;
  return *alias;
}

int branch_reassigned_bad(bool choose) {
  int current = 5;
  int other = 9;
  int* alias = &current;
  if (choose)
    alias = &other;
  else
    alias = &other;
  current = 7;
  return *alias;
}

int unknown_reassigned_bad(int* external) {
  int current = 5;
  int* alias = &current;
  current = 7;
  alias = external;
  return *alias;
}

int read_then_write_bad() {
  int current = 5;
  int* alias = &current;
  int saved = *alias;
  current = 7;
  return saved;
}

void write_only_alias_bad() {
  int current = 5;
  int* alias = &current;
  current = 7;
  *alias = 9;
}
void escaped_before_call_ok() {
  int current = 5;
  int* alias = &current;
  int** indirect = &alias;
  current = 7;
  consume_indirect(indirect);
}

void escaped_after_call_ok() {
  int current = 5;
  int* alias = &current;
  consume(alias);
  current = 7;
}

int indirect_reassigned_bad() {
  int current = 5;
  int other = 9;
  int* alias = &current;
  int** indirect = &alias;
  current = 7;
  *indirect = &other;
  return *alias;
}

bool pointer_comparison_only_bad() {
  int current = 5;
  int* alias = &current;
  current = 7;
  return alias != nullptr;
}

void reassigned_before_call_bad() {
  int current = 5;
  int other = 9;
  int* alias = &current;
  current = 7;
  alias = &other;
  consume(alias);
}
int read_after_value_marker_ok() {
  int current = 5;
  int* alias = &current;
  (void)alias;
  unrelated();
  current = 7;
  return *alias;
}

void unused_after_value_marker_bad() {
  int current = 5;
  int* alias = &current;
  (void)alias;
  current = 7;
}
int zero_index_reassigned_bad() {
  int current = 5;
  int other = 9;
  int* alias = &current;
  int** indirect = &alias;
  current = 7;
  indirect[0] = &other;
  return *alias;
}

struct Holder {
  int* alias;
};

int field_reassigned_bad() {
  int current = 5;
  int other = 9;
  Holder holder;
  holder.alias = &current;
  current = 7;
  holder.alias = &other;
  return *holder.alias;
}

void unused_field_alias_bad() {
  int current = 5;
  Holder holder;
  holder.alias = &current;
  current = 7;
  (void)holder.alias;
}
void unused_captured_alias_bad() {
  int current = 5;
  int* alias = &current;
  auto callback = [alias] { return *alias; };
  current = 7;
  (void)callback;
}

void store_before_unused_capture_bad() {
  int current = 5;
  int* alias = &current;
  current = 7;
  auto callback = [alias] { return *alias; };
  (void)callback;
}
} // namespace pointer_alias
