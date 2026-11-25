/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// An exception specification is not part of the identity a function-type template argument gets.
// A translated function type keeps its parameter and return types and nothing else, so
// `R(A...)` and `R(A...) noexcept` still produce one type name and share one type environment
// entry, as every pair of specializations did before that identity existed. The outer
// specialization here holds the inner one by value, so the shared entry names itself as the type
// of its own field: the same self-referential shape that overflowed the stack.
//
// What keeps this translation unit alive is therefore the cycle guard, not the signature. The
// guard refuses to enumerate the fields of a type that contains itself, so the frontend gives up
// on this one procedure with an incorrect assumption and translates the rest of the file. That
// cost is the point of the test: it pins what actually happens on this shape today.
//
// This file needs C++17, where an exception specification is part of the function type and the
// two partial specializations below can be written at all. In C++11 they are one specialization
// and the second is a redefinition, which is why the parameter-axis version of this shape lives
// in cpp/frontend instead.

template <typename Sig>
struct UniqueFn;

template <typename R, typename... A>
struct UniqueFn<R(A...)> {
  int x;
};

template <typename R, typename... A>
struct UniqueFn<R(A...) noexcept> {
  UniqueFn<R(A...)> impl_;
};

struct NoexceptHost {
  UniqueFn<void() noexcept> f;
};

NoexceptHost* zero_init_noexcept_function_type_template_arg_bad() {
  return new NoexceptHost();
}

// The control, on the axis the identity change does cover, in the same standard and the same
// file: the same self-referential arrangement, but the two specializations differ in a PARAMETER
// type. There the signature separates them, so the entry does not name itself, no guard is
// needed, and this procedure translates normally. The pair is what makes the case above readable:
// the difference between them is the exception specification and nothing else.
template <typename Sig>
struct SigFn;

template <typename R>
struct SigFn<R(int)> {
  int x;
};

template <typename R>
struct SigFn<R()> {
  SigFn<R(int)> impl_;
};

struct SigHost {
  SigFn<void()> f;
};

SigHost* zero_init_param_function_type_template_arg_ok() {
  return new SigHost();
}
