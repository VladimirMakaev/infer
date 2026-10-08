/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <atomic>
#include <functional>
#include <string>
#include <utility>

void get_closure(std::function<int()> closure);

enum my_enum {
  my_enum_1 = 0,
  my_enum_2 = 1,
};

extern my_enum get_my_enum();

class Uninit {
  void closure_call_ok() {
    auto closure = [this]() { return 5; };
    get_closure(closure);
  }

  class MyClass {
   public:
    int i;
    int j;

    MyClass() {}
    MyClass(int x, int y) : i(x), j(y) {}
  };

  void init_by_store(MyClass* x) {
    MyClass y{3, 5};
    reinterpret_cast<std::atomic<MyClass>*>(x)->store(
        y, std::memory_order_release);
  }

  void call_init_by_store_ok() {
    MyClass x;
    init_by_store(&x);
    int y = x.i;
  }

  MyClass get_MyClass() {
    switch (get_my_enum()) {
      case (my_enum_1):
        return MyClass{1, 2};
        break;
      case (my_enum_2):
        return MyClass{1, 2};
        break;
    }
  }

  void call_get_MyClass_ok() { int x = get_MyClass().i; }

  MyClass get_MyClass_param(my_enum my_enum) {
    switch (my_enum) {
      case (my_enum_1):
        return MyClass{1, 2};
        break;
      case (my_enum_2):
        return MyClass{1, 2};
        break;
    }
  }

  void call_get_MyClass_param_ok(my_enum my_enum) {
    int x = get_MyClass_param(my_enum).i;
  }

  MyClass get_MyClass_infeasible_default() {
    switch (get_my_enum()) {
      case (my_enum_1):
        return MyClass{1, 2};
        break;
      case (my_enum_2):
        return MyClass{1, 2};
        break;
      default:
        // infeasible
        break;
    }
  }

  void call_get_MyClass_infeasible_default_ok() {
    int x = get_MyClass_infeasible_default().i;
  }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-type"
  MyClass get_MyClass_feasible_default() {
    switch (get_my_enum()) {
      case (my_enum_1):
        return MyClass{1, 2};
        break;
      default:
        // feasible
        break;
    }
  }
#pragma clang diagnostic pop

  void call_get_MyClass_feasible_default_bad() {
    int x = get_MyClass_feasible_default().i;
  }

  std::function<MyClass()> unknown;

  MyClass havoc_return_param() { return unknown(); }

  int call_havoc_return_param_ok() {
    MyClass x = havoc_return_param();
    return x.i;
  }
};

class Uninit2 {
 public:
  int f1;
  int f2;

  Uninit2() {}

  void may_read_f1_empty(std::string s) {
    if (s.empty()) {
      int x = f1;
    }
  }

  void not_read_f1_ok() {
    Uninit2 o;
    o.may_read_f1_empty("non empty string");
  }

  void read_f1_bad() {
    Uninit2 o;
    o.may_read_f1_empty(std::string());
  }

  void may_read_f2_length(std::string s) {
    if (s.length() == 0) {
      int x = f2;
    }
  }

  void not_read_f2_ok() {
    Uninit2 o;
    o.may_read_f2_length("non empty string");
  }

  void read_f2_bad() {
    Uninit2 o;
    o.may_read_f2_length("");
  }
};

void unknown_call_lambda(std::function<void()> f);

int init_by_capture_good() {
  int x;
  unknown_call_lambda([&]() { x = 42; });
  return x;
}

void init_param(int* p) { p[0] = 42; }

int init_in_callee_ok() {
  int x;
  init_param(&x);
  return x;
}

class Nested {
 public:
  int i;
  Uninit2 mc;
};

void unknown_init_nested(Nested& x);

int read_nested(Nested& x) {
  unknown_init_nested(x);
  return x.mc.f1;
}

int call_read_nested_ok() {
  Nested x;
  return read_nested(x);
}

class Uninit3 {
 public:
  int f1;
  int f2;
};

class Uninit4 {
  Uninit3& uninit3_;
  int x;

 public:
  Uninit4(Uninit3& uninit3) : uninit3_{uninit3} { Uninit3 dummy = uninit3_; }
};

void construct_unint4_ok(Uninit3 uninit3) { Uninit4 uninit4(uninit3); }

int dummy_func(int);

void comma_operator_ok() {
  int i, j;
  i = ({ dummy_func(42); }), j = ({ dummy_func(i); });
}

class UninitUserProvided {
 private:
  int p;

 public:
  int x;
  int y = 42;

  UninitUserProvided() {}
};

int uninit_user_provided_bad() {
  UninitUserProvided a{};
  return a.x;
}

class UninitDefault {
 private:
  int p;

 public:
  int x;
  int y = 42;

  UninitDefault() = default;
};

int uninit_default_ok() {
  UninitDefault a{}; // a.x is initialized by the value-initialization rule.
  return a.x;
}

struct PartialStruct {
  int a;
  float b;
  int c;
};

void take_struct_by_value(PartialStruct s);

// copying a partially initialized struct is not a read of its fields
void pass_partially_initialized_by_value_ok() {
  PartialStruct s;
  s.b = 1.0f;
  take_struct_by_value(s);
}

void assign_partially_initialized_ok(PartialStruct& out) {
  PartialStruct s;
  s.c = 2;
  out = s;
}

int read_uninitialized_field_of_copy_bad() {
  PartialStruct s;
  s.b = 1.0f;
  PartialStruct t = s;
  return t.a;
}

float read_initialized_field_of_copy_ok() {
  PartialStruct s;
  s.b = 1.0f;
  PartialStruct t = s;
  return t.b;
}

int read_uninitialized_field_after_move_bad() {
  PartialStruct s;
  s.b = 1.0f;
  PartialStruct t = std::move(s);
  return t.c;
}

int read_uninitialized_field_after_assign_bad() {
  PartialStruct t{1, 2.0f, 3};
  assign_partially_initialized_ok(t);
  return t.a;
}

int read_initialized_field_after_assign_ok() {
  PartialStruct t;
  assign_partially_initialized_ok(t);
  return t.c;
}

struct PartialBase {
  int x;
  int y;
};

struct PartialDerived : PartialBase {
  PartialStruct inner;
  int z;
};

int read_uninitialized_nested_field_of_copy_bad() {
  PartialDerived d;
  d.inner.b = 1.0f;
  PartialDerived e = d;
  return e.inner.a;
}

int read_uninitialized_base_field_of_copy_bad() {
  PartialDerived d;
  d.x = 1;
  PartialDerived e;
  e = d;
  return e.y;
}

PartialStruct copy_struct(const PartialStruct& s) { return s; }

// the copy made by the callee does not tell the caller which fields it left
// uninitialized
int FN_read_uninitialized_field_of_copy_in_callee_bad() {
  PartialStruct s;
  s.b = 1.0f;
  PartialStruct t = copy_struct(s);
  return t.a;
}

struct PartialWithString {
  int a;
  std::string s;
  int c;
};

void take_with_string_by_value(PartialWithString s);

// the same for the member-wise copies of classes that are not trivially
// copyable
void pass_partially_initialized_with_string_by_value_ok() {
  PartialWithString s;
  s.c = 1;
  take_with_string_by_value(s);
  s.c = 2;
}

void assign_partially_initialized_with_string_ok(PartialWithString& out) {
  PartialWithString s;
  s.c = 1;
  out = s;
  s.c = 2;
}

int read_uninitialized_field_of_copy_with_string_bad() {
  PartialWithString s;
  s.c = 1;
  PartialWithString t = s;
  t.c = 2;
  return t.a;
}

int read_initialized_field_of_copy_with_string_ok() {
  PartialWithString s;
  s.c = 1;
  PartialWithString t = s;
  s.c = 2;
  return t.c;
}

int read_uninitialized_field_after_move_with_string_bad() {
  PartialWithString s;
  s.c = 1;
  PartialWithString t = std::move(s);
  return t.a;
}

int read_uninitialized_field_after_assign_with_string_bad() {
  PartialWithString t;
  t.a = 1;
  t.c = 2;
  assign_partially_initialized_with_string_ok(t);
  return t.a;
}

template <typename T>
struct PartialTemplate {
  T a;
  std::string s;
};

int read_uninitialized_field_of_template_copy_bad() {
  PartialTemplate<int> s;
  PartialTemplate<int> t = s;
  t.s = "x";
  return t.a;
}

struct PartialBaseWithString {
  int x;
  std::string s;
};

struct PartialDerivedWithString : PartialBaseWithString {
  PartialWithString inner;
  int z;
};

int read_initialized_field_of_derived_copy_with_string_ok() {
  PartialDerivedWithString d;
  d.z = 1;
  PartialDerivedWithString e = d;
  d.z = 2;
  return e.z;
}

int read_uninitialized_base_field_of_copy_with_string_bad() {
  PartialDerivedWithString d;
  d.z = 1;
  PartialDerivedWithString e = d;
  e.z = 2;
  return e.x;
}

int read_uninitialized_nested_field_of_assign_with_string_bad() {
  PartialDerivedWithString d;
  d.inner.c = 1;
  PartialDerivedWithString e;
  e.inner.a = 1;
  e = d;
  e.z = 2;
  return e.inner.a;
}

struct UserCopied {
  int x;
  std::string s;
  UserCopied() {}
  UserCopied(const UserCopied& other) : x(other.x), s(other.s) {}
};

void take_user_copied_by_value(UserCopied u);

// the reads of user-written copy constructors are still checked
void pass_user_copied_by_value_bad() {
  UserCopied u;
  take_user_copied_by_value(u);
  u.x = 1;
}

struct HasUserCopiedField {
  UserCopied u;
  int y;
};

void take_has_user_copied_field_by_value(HasUserCopiedField h);

void pass_user_copied_field_by_value_bad() {
  HasUserCopiedField h;
  h.y = 1;
  take_has_user_copied_field_by_value(h);
  h.y = 2;
}

struct ResetOnCopy {
  int x;
  std::string s;
  ResetOnCopy() {}
  ResetOnCopy(const ResetOnCopy& other) : x(0), s(other.s) {}
};

struct HasResetOnCopyField {
  ResetOnCopy r;
  std::string t;
};

int read_field_reset_by_user_copy_ok() {
  HasResetOnCopyField h;
  HasResetOnCopyField g = h;
  h.t = "x";
  return g.r.x;
}

struct ResetOnCopyWithDefaultArg {
  int x;
  std::string s;
  ResetOnCopyWithDefaultArg() {}
  ResetOnCopyWithDefaultArg(const ResetOnCopyWithDefaultArg& other, int = 0)
      : x(0), s(other.s) {}
};

struct HasResetOnCopyWithDefaultArgField {
  ResetOnCopyWithDefaultArg r;
  std::string t;
};

int read_field_reset_by_user_copy_with_default_arg_ok() {
  HasResetOnCopyWithDefaultArgField h;
  HasResetOnCopyWithDefaultArgField g = h;
  h.t = "x";
  return g.r.x;
}

struct DefaultedCopy {
  int x;
  std::string s;
  DefaultedCopy() {}
  DefaultedCopy(const DefaultedCopy&) = default;
};

void take_defaulted_copy_by_value(DefaultedCopy d);

// explicitly defaulted copy constructors are checked like user-written ones
void FP_pass_defaulted_copy_by_value_ok() {
  DefaultedCopy d;
  take_defaulted_copy_by_value(d);
  d.x = 1;
}
