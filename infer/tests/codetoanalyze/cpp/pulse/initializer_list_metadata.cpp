/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
#include <initializer_list>
#include <utility>
#include <cstddef>

namespace initializer_list_metadata {

using List = std::initializer_list<int>;
std::size_t count_ref(const List& xs) { return xs.size(); }
std::size_t count_val(List xs) { return xs.size(); }
void assign(List& dst, const List& src) { dst = src; }
List echo(List xs) { return xs; }
List opaque();
int local_copy_ok() {
  List a = {1, 2, 3};
  auto b = a;
  return b.size() == 3 ? 0 : *static_cast<int*>(nullptr);
}
int assigned_nonempty_ok() {
  List a = {1, 2, 3};
  List b;
  assign(b, a);
  return b.size() == 3 ? 0 : *static_cast<int*>(nullptr);
}
int assigned_empty_ok() {
  List a;
  List b = {1, 2, 3};
  assign(b, a);
  return b.size() == 0 ? 0 : *static_cast<int*>(nullptr);
}
int independent_copy_ok() {
  List a = {1, 2};
  auto b = a;
  List c = {9};
  a = c;
  return b.size() == 2 && a.size() == 1 ? 0 : *static_cast<int*>(nullptr);
}
int shallow_copy_ok() {
  List a = {1, 2};
  auto b = a;
  return b.begin() == a.begin() && b.end() == a.end()
             ? 0
             : *static_cast<int*>(nullptr);
}
int alias_assignment_ok() {
  List a = {1, 2};
  auto& b = a;
  List c = {7, 8, 9};
  b = c;
  return a.size() == 3 ? 0 : *static_cast<int*>(nullptr);
}
int self_assignment_ok() {
  List a = {1, 2};
  a = a;
  return a.size() == 2 ? 0 : *static_cast<int*>(nullptr);
}
int returned_copy_ok() {
  List a = {1, 2};
  auto b = echo(a);
  return b.size() == 2 ? 0 : *static_cast<int*>(nullptr);
}
int moved_copy_ok() {
  List a = {1, 2};
  auto b = std::move(a);
  return b.size() == 2 && a.size() == 2 ? 0 : *static_cast<int*>(nullptr);
}
int empty_size_ok() {
  List a;
  return a.size() == 0 ? 0 : *static_cast<int*>(nullptr);
}
int empty_equality_ok() {
  List a;
  return a.begin() == a.end() ? 0 : *static_cast<int*>(nullptr);
}
int empty_copy_ok() {
  List a;
  List b = a;
  return b.size() == 0 ? 0 : *static_cast<int*>(nullptr);
}
int empty_front_bad() {
  List a;
  return *a.begin();
}
int empty_copy_front_bad() {
  List a;
  List b = a;
  return *b.begin();
}
int unknown_empty_bad(List a) {
  return a.size() == 0 ? *static_cast<int*>(nullptr) : 0;
}
int unknown_nonempty_bad(List a) {
  return a.size() != 0 ? *static_cast<int*>(nullptr) : 0;
}
int unknown_copy_empty_bad(List a) {
  List b = a;
  return b.size() == 0 ? *static_cast<int*>(nullptr) : 0;
}
int unknown_copy_nonempty_bad(List a) {
  List b = a;
  return b.size() != 0 ? *static_cast<int*>(nullptr) : 0;
}
int opaque_empty_bad() {
  auto a = opaque();
  return a.size() == 0 ? *static_cast<int*>(nullptr) : 0;
}
int opaque_nonempty_bad() {
  auto a = opaque();
  return a.size() != 0 ? *static_cast<int*>(nullptr) : 0;
}
struct Holder {
  List xs;
};
int member_ok() {
  Holder a{{1, 2, 3}};
  Holder b = a;
  return b.xs.size() == 3 ? 0 : *static_cast<int*>(nullptr);
}
int array_members_ok() {
  List a[2] = {{1, 2}, {3}};
  return a[0].size() == 2 && a[1].size() == 1 ? 0 : *static_cast<int*>(nullptr);
}
int const_ref_literal_ok() {
  return count_ref({1, 2, 3}) == 3 ? 0 : *static_cast<int*>(nullptr);
}
int by_value_literal_ok() {
  return count_val({1, 2, 3}) == 3 ? 0 : *static_cast<int*>(nullptr);
}
int copied_ref_call_ok() {
  List a = {1, 2, 3};
  auto b = a;
  return count_ref(b) == 3 ? 0 : *static_cast<int*>(nullptr);
}

template <class T>
struct Slice {
  const T* data;
  std::size_t length;
  Slice(const std::initializer_list<T>& values)
      : data(values.begin() == values.end() ? nullptr : values.begin()),
        length(values.size()) {}
  const T& front() const { return data[0]; }
};
int read_front(Slice<int> values) { return values.front(); }
int literal_slice_ok() { return read_front({1, 2, 3}); }
int empty_slice_bad() { return read_front({}); }
int local_slice_ok() {
  List values = {1, 2, 3};
  return read_front(values);
}
int copied_slice_ok() {
  List values = {1, 2, 3};
  List copy = values;
  return read_front(copy);
}
int unknown_slice(const List& values) { return read_front(values); }
int unknown_slice_empty_bad() {
  List values;
  return unknown_slice(values);
}
int unknown_slice_nonempty_ok() {
  List values = {7};
  return unknown_slice(values);
}
int elements_preserved_ok() {
  return read_front({7}) == 7 ? 0 : *static_cast<int*>(nullptr);
}
int empty_end_bad() {
  List values;
  return *values.end();
}

void change(List&);
int aggregate_copy_independence_ok() {
  Holder original{{1, 2}};
  Holder copy = original;
  List empty;
  copy.xs = empty;
  return original.xs.size() == 2 && copy.xs.size() == 0
             ? 0
             : *static_cast<int*>(nullptr);
}
int opaque_mutation_copy_ok() {
  List original = {1, 2};
  List copy = original;
  change(copy);
  return original.size() == 2 ? 0 : *static_cast<int*>(nullptr);
}
int pointer_alias_ok() {
  List values = {1, 2};
  List* pointer = &values;
  return pointer->size() == 2 ? 0 : *static_cast<int*>(nullptr);
}

} // namespace initializer_list_metadata
