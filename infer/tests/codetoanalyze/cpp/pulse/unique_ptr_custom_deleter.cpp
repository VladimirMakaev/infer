/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Regression cover for std::unique_ptr with a CUSTOM DELETER, i.e. the
// two-template-argument specialisation: a function pointer such as decltype(&fclose),
// and a stateless functor. A null check on such a pointer has to be remembered just as
// it is for the default deleter, through get(), a move, and reset().
//
// The pair that makes this file worth having is unchecked_bad / checked_ok below: they
// are the same code apart from the check. Without the _bad half, the _ok half would
// stay silent even if the pointer were never tracked at all, and the file would pass
// while testing nothing.
//
// This directory is also compiled with -std=c++11 by the pulse-11 case, so everything
// here stays inside C++11.

#include <cstdio>
#include <memory>
#include <utility>

namespace unique_ptr_custom_deleter {

struct T {
  int v;
};
void sink(int);

struct Deleter {
  void operator()(T* t) const { delete t; }
};

// ----------------------- function-pointer deleter -----------------------

// fopen may return null and nothing rules it out here
void fnptr_unchecked_bad(const char* path, char* buf, int n) {
  std::unique_ptr<FILE, decltype(&fclose)> f(fopen(path, "r"), &fclose);
  fgets(buf, n, f.get());
}

// the same, with the check: the null is ruled out, so there is nothing to report
void fnptr_checked_ok(const char* path, char* buf, int n) {
  std::unique_ptr<FILE, decltype(&fclose)> f(fopen(path, "r"), &fclose);
  if (!f) {
    return;
  }
  fgets(buf, n, f.get());
}

// the same check written the other way round
void fnptr_checked_truthy_ok(const char* path, char* buf, int n) {
  std::unique_ptr<FILE, decltype(&fclose)> f(fopen(path, "r"), &fclose);
  if (f) {
    fgets(buf, n, f.get());
  }
}

void fnptr_null_deref_bad(char* buf, int n) {
  std::unique_ptr<FILE, decltype(&fclose)> f(nullptr, &fclose);
  fgets(buf, n, f.get());
}

// ----------------------- stateless functor deleter -----------------------

void functor_checked_ok(T* raw) {
  std::unique_ptr<T, Deleter> u(raw);
  if (!u) {
    return;
  }
  sink(u.get()->v);
}

void functor_null_deref_bad() {
  std::unique_ptr<T, Deleter> u(nullptr);
  sink(u.get()->v);
}

// ----------------------- move -----------------------

void move_then_checked_ok(T* raw) {
  std::unique_ptr<T, Deleter> a(raw);
  std::unique_ptr<T, Deleter> b(std::move(a));
  if (!b) {
    return;
  }
  sink(b.get()->v);
}

void use_after_move_bad(T* raw) {
  std::unique_ptr<T, Deleter> a(raw);
  std::unique_ptr<T, Deleter> b(std::move(a));
  sink(a.get()->v);
}

// ----------------------- reset -----------------------

void reset_then_checked_ok(T* raw, T* other) {
  std::unique_ptr<T, Deleter> u(raw);
  u.reset(other);
  if (!u) {
    return;
  }
  sink(u.get()->v);
}

void reset_null_then_checked_ok(T* raw) {
  std::unique_ptr<T, Deleter> u(raw);
  u.reset(nullptr);
  if (!u) {
    return;
  }
  sink(u.get()->v);
}

void reset_null_deref_bad(T* raw) {
  std::unique_ptr<T, Deleter> u(raw);
  u.reset(nullptr);
  sink(u.get()->v);
}

// ----------------------- default deleter, for comparison -----------------------

void default_deleter_unchecked_bad(const char* path, char* buf, int n) {
  std::unique_ptr<FILE> f(fopen(path, "r"));
  fgets(buf, n, f.get());
}

void default_deleter_checked_ok(T* raw) {
  std::unique_ptr<T> u(raw);
  if (!u) {
    return;
  }
  sink(u.get()->v);
}

} // namespace unique_ptr_custom_deleter
