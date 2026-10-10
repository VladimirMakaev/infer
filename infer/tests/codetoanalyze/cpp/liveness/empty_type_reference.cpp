/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
namespace empty_type_reference {
template <class T> int type_key() { return 17; }
struct Empty {};
struct Stateful { int field; explicit Stateful(int v) : field(v) {} };
struct ExplicitEmpty { explicit ExplicitEmpty(int v) { int copied = v; (void)copied; } };
struct DefaultField { int field = 17; };
struct ExplicitDefault { ExplicitDefault() = default; };
struct Derived : Empty {};
struct VirtualEmpty { virtual int read() { return 1; } };

int decltype_only_ok() {
  struct { int operator()() const noexcept { return 1; } } token;
  return type_key<decltype(token)>();
}
int unused_empty_bad() { Empty token; return 1; }
int stateful_aggregate_ok() {
  struct State { int field; } token{17};
  return type_key<decltype(token)>();
}
int scalar_type_only_bad() {
  int value = 11;
  return type_key<decltype(value)>();
}
int overwritten_bad() {
  int value = 11;
  using Type = decltype(value);
  value = Type{22};
  return value;
}
int stateful_constructor_bad() {
  Stateful token(17);
  return type_key<decltype(token)>();
}
int explicit_empty_constructor_bad() {
  ExplicitEmpty token(17);
  return type_key<decltype(token)>();
}
int default_field_constructor_bad() {
  DefaultField token;
  return type_key<decltype(token)>();
}
int sizeof_only_ok() { Empty token; return sizeof(token); }
int noexcept_only_ok() {
  struct EmptyCallable { int operator()() const noexcept { return 1; } } token;
  return noexcept(token());
}
int typedef_only_ok() {
  typedef Empty Alias;
  Alias token;
  return type_key<decltype(token)>();
}
int parenthesized_decltype_ok() { Empty token; return type_key<decltype((token))>(); }
int copied_empty_bad() {
  Empty source;
  Empty token(source);
  return type_key<decltype(token)>();
}
int explicit_defaulted_bad() {
  ExplicitDefault token;
  return type_key<decltype(token)>();
}
int inherited_empty_bad() {
  Derived token;
  return type_key<decltype(token)>();
}
int virtual_empty_bad() {
  VirtualEmpty token;
  return type_key<decltype(token)>();
}
int unused_callable_bad() {
  struct { int operator()() const noexcept { return 1; } } token;
  return 1;
}

template <class T> struct EmptyTemplate {};
template <class T> struct FieldTemplate { T field = 17; };
struct Forward;
struct Forward {};
int template_empty_ok() {
  EmptyTemplate<int> token;
  return type_key<decltype(token)>();
}
int template_field_bad() {
  FieldTemplate<int> token;
  return type_key<decltype(token)>();
}
int forward_defined_ok() {
  Forward token;
  return type_key<decltype(token)>();
}
}
