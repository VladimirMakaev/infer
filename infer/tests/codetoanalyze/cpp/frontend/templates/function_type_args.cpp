/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// A function type used as a template argument has to keep its signature, or every
// specialization of the template collapses onto one type name and shares one tenv entry.

template <typename Sig>
struct Fn;

template <typename R>
struct Fn<R(int)> {
  int x;
};

template <typename R>
struct Fn<R()> {
  Fn<R(int)> impl_;
};

struct Host {
  Fn<void()> f;
};

// Translating `Fn<R()>` and `Fn<R(int)>` to the same name left the tenv holding a single
// `Fn<_fn_>` whose field `impl_` had type `Fn<_fn_>`, i.e. itself. Zero-initializing `Host`
// walked `impl_` forever and overflowed the stack, which loses the whole translation unit and
// not just this procedure.
Host* zero_init_function_type_template_arg_bad() { return new Host(); }

// Same shape with template arguments that already had distinct names.
template <typename T>
struct Box;

template <>
struct Box<int> {
  int x;
};

template <>
struct Box<char> {
  Box<int> impl_;
};

struct PlainHost {
  Box<char> f;
};

PlainHost* zero_init_distinct_template_arg_ok() { return new PlainHost(); }

// The collision was not only a crash: the two specializations have different members, so field
// resolution through the shared entry could silently pick the wrong one. `Fn<void(int)>` has `x`
// and `Fn<void()>` has `impl_`; both must resolve.
int member_resolution_through_function_type_args_ok() {
  Fn<void(int)> inner;
  Fn<void()> outer;
  inner.x = 1;
  outer.impl_.x = 2;
  return inner.x + outer.impl_.x;
}
