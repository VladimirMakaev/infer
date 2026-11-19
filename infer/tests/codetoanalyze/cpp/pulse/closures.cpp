/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
#include <functional>

struct S {
  int f;

  S() { f = 1; }
  ~S() {}
};

int ref_capture_destroy_invoke_bad() {
  std::function<int()> f;
  {
    S s;
    f = [&s] { return s.f; };
  } // destructor for s called here
  return f(); // s used here
}

int implicit_ref_capture_destroy_invoke_bad() {
  std::function<int()> f;
  {
    auto s = S();
    f = [&] { return s.f; };
  }
  return f();
}

// FN in C++14
int reassign_lambda_capture_destroy_invoke_bad() {
  std::function<int()> f;
  {
    auto s = S();
    // this is a copy constructor in C++14, which pulse misses,
    // but it's a straight assignment in C++17, which pulse understands
    auto tmp = [&] { return s.f; };
    f = tmp;
  }
  return f();
}

int value_capture_destroy_invoke_ok() {
  std::function<int()> f;
  {
    S s;
    f = [s] { return s.f; };
  }
  return f();
}

int implicit_value_capture_destroy_invoke_ok() {
  std::function<int()> f;
  {
    S s;
    f = [=] { return s.f; };
  }
  return f();
}

int ref_capture_invoke_ok() {
  std::function<int()> f;
  int ret;
  {
    S s;
    f = [&s] { return s.f; };
    ret = f();
  }
  return ret;
}

void invoke_twice_ok() {
  std::function<int()> f;
  int ret;
  {
    S s;
    f = [&s] { return s.f; };
    f();
    f();
  }
}

void ref_capture_read_lambda_ok() {
  std::function<int()> f;
  int ret;
  {
    S s;
    f = [&s] { return s.f; };
  }
  auto tmp =
      f; // reading (but not invoking) the lambda doesn't use its captured vars
}

int delete_lambda_then_call_bad() {
  std::function<int()> lambda = [] { return 1; };
  lambda.~function();
  return lambda();
}

// need to treat escaping as a use in order to catch this
std::function<int()> FN_ref_capture_return_lambda_bad() {
  std::function<int()> f;
  int ret;
  {
    S s;
    f = [&s] { return s.f; };
  }
  return f; // if the caller invokes the lambda, it will try to read the invalid
            // stack address
}

int ref_capture_return_local_lambda_ok() {
  S x;
  auto f = [&x](void) -> S& {
    // do not report this because there is a good chance that this function will
    // only be used in the local scope
    return x;
  };
  return f().f;
}

S& ref_capture_return_local_lambda_bad() {
  S x;
  auto f = [&x](void) -> S& {
    // no way to know if ok here
    return x;
  };
  // woops, this returns a ref to a local!
  return f();
}

struct C {
  int val() const;
  ~C();
};

struct D {
  void add(int v);
  ~D();
};

void capture_multiple_vars_by_value_ok(C c, C c2) {
  auto f = [=]() -> D* {
    auto d = new D();
    d->add(c.val());
    d->add(c2.val());
    return d;
  };
}

void call_lambda_ok() {
  auto f = [](S* s) { int x = s->f; };
  S* s = new S();
  f(s);
  delete s;
}

void call_lambda_bad() {
  auto f = [](S* s) { int x = s->f; };
  S* s = new S();
  delete s;
  f(s);
}

void call_lambda_std_fun_bad() {
  std::function<void(S*)> f;
  f = [](S* s) { int x = s->f; };
  S* s = new S();
  delete s;
  f(s);
}

void call_std_fun_constructor_bad() {
  std::function<void(S*)> f1 = [](S* s) { int x = s->f; };
  std::function<void(S*)> f2 = f1;
  S* s = new S();
  delete s;
  f2(s);
}

void function_constructor_null_ok() { std::function<int()> f = nullptr; }

void function_assign_null_ok() {
  std::function<int()> f = [] { return 1; };
  f = nullptr;
}

void capture_by_value_ok() {
  int value = 5;
  auto f = [value]() -> int* { return new int(value); };
  value++;
  int* p = f();
  int* q = nullptr;
  if (*p != 5) {
    *q = 42;
  }
  delete p;
}

void capture_by_value_bad() {
  int value = 5;
  auto f = [value]() -> int* { return new int(value); };
  value++;
  int* p = f();
  int* q = nullptr;
  if (*p == 5) {
    *q = 42;
  }
  delete p;
}

void capture_by_ref_ok() {
  int value = 5;
  auto f = [&value]() -> int* { return new int(value); };
  value++;
  int* p = f();
  int* q = nullptr;
  if (*p != 6) {
    *q = 42;
  }
  delete p;
}

void capture_by_ref_bad() {
  int value = 5;
  auto f = [&value]() -> int* { return new int(value); };
  value++;
  int* p = f();
  int* q = nullptr;
  if (*p == 6) {
    *q = 42;
  }
  delete p;
}

void capture_by_value_init_ok() {
  int value = 5;
  auto f = [v = value]() -> int* { return new int(v); };
  value++;
  int* p = f();
  int* q = nullptr;
  if (*p != 5) {
    *q = 42;
  }
  delete p;
}

void capture_by_value_init_bad() {
  int value = 5;
  auto f = [v = value]() -> int* { return new int(v); };
  value++;
  int* p = f();
  int* q = nullptr;
  if (*p == 5) {
    *q = 42;
  }
  delete p;
}

void capture_by_ref_init_ok() {
  int value = 5;
  auto f = [&v = value]() -> int* { return new int(v); };
  value++;
  int* p = f();
  int* q = nullptr;
  if (*p != 6) {
    *q = 42;
  }
  delete p;
}

void capture_by_ref_init_bad() {
  int value = 5;
  auto f = [&v = value]() -> int* { return new int(v); };
  value++;
  int* p = f();
  int* q = nullptr;
  if (*p == 6) {
    *q = 42;
  }
  delete p;
}

void ref_capture_by_value_ok() {
  int value = 5;
  int& ref = value;
  auto f = [ref]() -> int* { return new int(ref); };
  ref++;
  int* p = f();
  int* q = nullptr;
  if (*p != 5) {
    *q = 42;
  }
  delete p;
}

void ref_capture_by_value_bad() {
  int value = 5;
  int& ref = value;
  auto f = [ref]() -> int* { return new int(ref); };
  ref++;
  int* p = f();
  int* q = nullptr;
  if (*p == 5) {
    *q = 42;
  }
  delete p;
}

void ref_capture_by_ref_ok() {
  int value = 5;
  int& ref = value;
  auto f = [&ref]() -> int* { return new int(ref); };
  ref++;
  int* p = f();
  int* q = nullptr;
  if (*p != 6) {
    *q = 42;
  }
  delete p;
}

void ref_capture_by_ref_bad() {
  int value = 5;
  int& ref = value;
  auto f = [&ref]() -> int* { return new int(ref); };
  ref++;
  int* p = f();
  int* q = nullptr;
  if (*p == 6) {
    *q = 42;
  }
  delete p;
}

void struct_capture_by_ref_bad() {
  S s;
  auto f = [&s]() -> int* { return new int(s.f); };
  s.f = 5;
  int* p = f();
  int* q = nullptr;
  if (*p == 5) {
    *q = 42;
  }
  delete p;
}

void struct_capture_by_ref_ok() {
  S s;
  auto f = [&s]() -> int* { return new int(s.f); };
  s.f = 5;
  int* p = f();
  int* q = nullptr;
  if (*p != 5) {
    *q = 42;
  }
  delete p;
}

void struct_capture_by_val_bad() {
  S s;
  auto f = [s]() -> int* { return new int(s.f); };
  s.f = 5;
  int* p = f();
  int* q = nullptr;
  if (*p == 1) {
    *q = 42;
  }
  delete p;
}

void struct_capture_by_val_ok_FP() {
  S s;
  auto f = [s]() -> int* { return new int(s.f); };
  s.f = 5;
  int* p = f();
  int* q = nullptr;
  if (*p != 1) {
    *q = 42;
  }
  delete p;
}

S* update_inside_lambda_capture_and_init(S* s) {
  S* object = nullptr;
  auto f = [&o = object](S* s) { o = s; };
  f(s);
  return object;
}

int update_inside_lambda_capture_and_init_ok(S* param_s) {
  return update_inside_lambda_capture_and_init(param_s)->f;
}

S* update_inside_lambda_capture_only(S* s) {
  S* object = nullptr;
  /* FIXME: clang AST gives us `S*` for  variable `object` in the
     lambda's body, hence the translation misses one dereference */
  auto f = [&object](S* s) { object = s; };
  f(s);
  return object;
}

int update_inside_lambda_capture_only_ok(S* param_s) {
  return update_inside_lambda_capture_only(param_s)->f;
}

void call_argument(std::function<void(S*)> f, S* s) { f(s); }

S* update_inside_lambda_as_argument(S* s) {
  S* object = nullptr;
  auto f = [&o = object](S* s) { o = s; };
  call_argument(f, s);
  return object;
}

int update_inside_lambda_as_argument_ok(S* param_s) {
  return update_inside_lambda_as_argument(param_s)->f;
}

std::function<void()> get_lambda(bool b) {
  return [b]() -> void { return; };
}

void capture_false_by_value_ok() {
  const auto& f = get_lambda(false);
  f();
}

void FP_update_inside_lambda_visible_outside_ok() {
  int x = 0; // there are two variables x in the symbolic state `roots={ &x=v3,
             // &x=v1 }`
  auto f = [&xx = x]() {
    xx++;
  }; // one variable disappear because of dealloc instruction
  x = 7; // again there are two variables x in the symbolic state
  f();
  if (x != 8) {
    int* p = nullptr;
    *p = 42;
  }
}

int std_function_writes_through_pointer_arg_bad(
    const std::function<void(int*)>& f) {
  int x = 0;
  f(&x);
  if (x != 0) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}

int std_function_converted_const_pointer_arg_ok(
    const std::function<void(const int*)>& f) {
  int x = 0;
  f(&x);
  if (x != 0) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}

int identity(int x) { return x; }

bool unknown_condition();

void std_function_assign_maybe_null_function_pointer_ok(
    std::function<int(int)>& f) {
  f = unknown_condition() ? identity : nullptr;
}

std::function<int(int)> std_function_from_maybe_null_function_pointer_ok() {
  int (*fp)(int) = unknown_condition() ? identity : nullptr;
  return std::function<int(int)>(fp);
}

struct FunctionHolder {
  std::function<int(int)> f;
};

FunctionHolder std_function_copy_maybe_null_function_pointer_ok() {
  FunctionHolder holder;
  holder.f = unknown_condition() ? identity : nullptr;
  return holder;
}

int* return_null() { return nullptr; }

int global_int;

int* return_global() { return &global_int; }

int std_function_from_function_pointer_null_bad() {
  std::function<int*()> f = return_null;
  return *f();
}

int std_function_from_function_pointer_nonnull_ok() {
  std::function<int*()> f = return_global;
  return *f();
}

// the null dereference is reported in the version of the callee specialized for
// the default argument
int call_default_function_null_bad(
    const std::function<int*()>& f = return_null) {
  return *f();
}

int call_default_function() { return call_default_function_null_bad(); }

// A closure that captures a local by reference reads it when it runs, so an
// assignment made after the capture has to be visible through the capture.
void ref_capture_sees_later_assignment_ok() {
  int* p = nullptr;
  auto read = [&p] { return *p; };
  p = &global_int;
  read();
}

// The same, in the other direction: a null assigned after the capture is a real
// dereference of null and has to be reported.
int ref_capture_sees_later_null_bad() {
  int* p = &global_int;
  auto read = [&p] { return *p; };
  p = nullptr;
  return read();
}

// Control for the two above: a capture by value copies at capture time, so a
// later assignment is invisible to the closure by design.
int value_capture_ignores_later_assignment_ok() {
  int* p = &global_int;
  auto read = [p] { return *p; };
  p = nullptr;
  return read();
}

// Everything below needs generic lambdas, so it is compiled out of the C++11
// run of this directory (see ../pulse-11, which rebuilds these same sources
// with -std=c++11).
#if __cplusplus >= 201402L

// A generic lambda's by-reference captures must bind to the variable's address
// at every instantiation, not only when the lambda is called at one argument
// type. When it was called at two, the closure named a bare `operator()` that
// carried no captures, while the call operator had already been translated
// expecting addresses -- so a write through the capture went to the value's
// address. With the variable initialised to 0 that reads as a null
// dereference, and with any other value the write is simply lost.
struct GenericV1 {
  const char* name;
  unsigned rank;
};
struct GenericV2 {
  const char* name;
  unsigned rank;
};

unsigned generic_lambda_two_instantiations_ok(const GenericV1& s1,
                                              const GenericV2& s2,
                                              bool c) {
  const char* name = "init";
  unsigned rank = 0;
  auto fill = [&](const auto& v) {
    name = v.name;
    rank = v.rank;
  };
  if (c) {
    fill(s2);
  } else {
    fill(s1);
  }
  return rank;
}

// The other half of the same defect, and the one a null-free initialiser hides:
// the write has to REACH the caller. The next three go together and must be read
// together -- the first is what stops the other two passing vacuously.
//
// Two constraints learned the hard way while reducing this. The values written
// have to be ones Pulse can pin, so they are built locally rather than read from
// a reference parameter: `rank = v.rank` on an unconstrained `unsigned` leaves
// `rank == 7` satisfiable and both guards below report. And the lambda body must
// not branch on anything it reads through its own formal: a lambda whose body
// branches on a symbolic parameter silences downstream reports entirely (true on
// a plain non-generic lambda too, and unrelated to this fix), which would make
// every `_ok` here pass for the wrong reason.

// Path-survival control for the two below: unconditional dereference after the
// two-instantiation call. Before the fix this was silent -- the mis-bound store
// destroyed the caller's disjuncts -- so the guarded cases could not have been
// trusted on their own.
int generic_lambda_write_path_survives_bad(bool c) {
  GenericV1 s1;
  s1.rank = 1;
  GenericV2 s2;
  s2.rank = 2;
  unsigned rank = 7;
  auto fill = [&](const auto& v) { rank = v.rank; };
  if (c) {
    fill(s2);
  } else {
    fill(s1);
  }
  int* p = nullptr;
  return *p;
}

// The write reached on both arms => `rank` is 1 or 2 => the guard is refuted and
// the dereference is unreachable. If the capture were bound by value on either
// arm, `rank` would keep 7 there and this would report.
int generic_lambda_write_reaches_caller_ok(bool c) {
  GenericV1 s1;
  s1.rank = 1;
  GenericV2 s2;
  s2.rank = 2;
  unsigned rank = 7;
  auto fill = [&](const auto& v) { rank = v.rank; };
  if (c) {
    fill(s2);
  } else {
    fill(s1);
  }
  int* p = nullptr;
  if (rank == 7) {
    return *p;
  }
  return 0;
}

// The mirror of the one above, under the opposite guard: the write reached, so
// `rank != 7` holds and the dereference is reached. A silence here would mean
// the path was lost rather than the guard refuted.
int generic_lambda_write_reaches_caller_bad(bool c) {
  GenericV1 s1;
  s1.rank = 1;
  GenericV2 s2;
  s2.rank = 2;
  unsigned rank = 7;
  auto fill = [&](const auto& v) { rank = v.rank; };
  if (c) {
    fill(s2);
  } else {
    fill(s1);
  }
  int* p = nullptr;
  if (rank != 7) {
    return *p;
  }
  return 0;
}

// Control: one instantiation only. This already worked and must keep working.
unsigned generic_lambda_one_instantiation_ok(const GenericV1& s) {
  const char* name = nullptr;
  unsigned rank = 0;
  auto fill = [&](const auto& v) {
    name = v.name;
    rank = v.rank;
  };
  fill(s);
  return rank;
}

// Control: two lambdas with written-out parameter types, so neither is generic.
unsigned generic_lambda_typed_pair_ok(const GenericV1& s1,
                                      const GenericV2& s2,
                                      bool c) {
  const char* name = nullptr;
  unsigned rank = 0;
  auto f1 = [&](const GenericV1& v) {
    name = v.name;
    rank = v.rank;
  };
  auto f2 = [&](const GenericV2& v) {
    name = v.name;
    rank = v.rank;
  };
  if (c) {
    f2(s2);
  } else {
    f1(s1);
  }
  return rank;
}

// TP control: a real null dereference through a capture of a generic lambda is
// still reported, so the fix suppresses nothing it should not.
int generic_lambda_two_instantiations_null_bad(const GenericV1& s1,
                                               const GenericV2& s2,
                                               bool c) {
  int* p = nullptr;
  auto deref = [&](const auto& v) { return *p + static_cast<int>(v.rank); };
  return c ? deref(s2) : deref(s1);
}

// Three instantiations of one lambda, each writing a value only its own
// specialisation can produce, so "every call site kept its own write" is
// checkable rather than assumed. The three `_bad` cases are what make the `_ok`
// mean something: were the path lost at the call instead of the guard refuted,
// all three would go silent and the `_ok` would still pass.
struct TriA {
  unsigned rank;
};
struct TriB {
  unsigned rank;
};
struct TriC {
  unsigned rank;
};

unsigned tri_pick(const TriA&) { return 10; }
unsigned tri_pick(const TriB&) { return 20; }
unsigned tri_pick(const TriC&) { return 30; }

unsigned generic_lambda_three_instantiations_write(int c) {
  TriA a;
  TriB b;
  TriC d;
  unsigned rank = 7;
  auto fill = [&](const auto& v) { rank = tri_pick(v); };
  if (c == 0) {
    fill(a);
  } else if (c == 1) {
    fill(b);
  } else {
    fill(d);
  }
  return rank;
}

// Each arm must stay reachable, i.e. each call site ran its own instantiation.
int generic_lambda_three_instantiations_first_bad(int c) {
  unsigned rank = generic_lambda_three_instantiations_write(c);
  int* p = nullptr;
  if (rank == 10) {
    return *p;
  }
  return 0;
}

int generic_lambda_three_instantiations_second_bad(int c) {
  unsigned rank = generic_lambda_three_instantiations_write(c);
  int* p = nullptr;
  if (rank == 20) {
    return *p;
  }
  return 0;
}

int generic_lambda_three_instantiations_third_bad(int c) {
  unsigned rank = generic_lambda_three_instantiations_write(c);
  int* p = nullptr;
  if (rank == 30) {
    return *p;
  }
  return 0;
}

// No arm lost its write (none leaves `rank` at 7) and no arm borrowed another's
// value, since nothing outside the three is producible.
int generic_lambda_three_instantiations_ok(int c) {
  unsigned rank = generic_lambda_three_instantiations_write(c);
  int* p = nullptr;
  if (rank != 10 && rank != 20 && rank != 30) {
    return *p;
  }
  return 0;
}

#endif // __cplusplus >= 201402L
