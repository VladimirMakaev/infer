/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <functional>

namespace folly {
template <class Value>
class Optional {
 public:
  Optional(const Optional& src);
  bool has_value();
  Value* operator->();
  const Value* operator->() const;

 private:
  Value v;
};

namespace coro {
template <class Value>
class Task {};
} // namespace coro
} // namespace folly

namespace const_refable {

struct Arr {
  int arr[2];
  std::vector<int> vec;
};

// The correct use of const reference when parameter is not modified
int read_first_ok(const Arr& a) { return a.vec[0]; }

// Non-const references are not reported on
std::string non_const_ref_ok(std::string& str) { return str + str; }

// R-value references are not reported on
void rvalue_ref_ok(Arr&& a) { auto& cpy = a; }

// vec is not modified, so should be const reference.
int read_first_bad(Arr a) { return a.vec[0]; }

int modify_first_ok(Arr a) {
  a.arr[0] += 8;
  return a.arr[0];
}

void ref_modified_ok(std::vector<int> vec) {
  auto& cpy = vec;
  cpy[0] += 8;
}

int get_first(const std::vector<int>& vec) { return vec[0]; }

void interprocedural_read_bad(std::vector<int> vec) {
  int first = get_first(vec);
}

// Sum vec by reading vector elements
int sum_in_loop_bad(std::vector<int> vec) {
  int total = 0;
  for (const int& x : vec)

    total += x; // vec is not modified here
  return total;
}

int sum_set_in_loop_bad(std::set<int> set) {
  int total = 0;
  for (int x : set) {
    total += x;
  }
  return total;
}

int erase_first_ok(std::set<int> set) {
  set.erase(set.begin());
  return set.size();
}

// anonymous parameters usually exist to satisfy the signature for virtual
// functions. If we recommend const ref, the signature would not match and
// making the base function const-refable might not be possible since it might
// be using the parameter.
void some_fun_ok(std::vector<int> /*unnamed_param*/) {}

void pass_rvalue_ref(std::set<int>&& x) {}

// params which are passed-by-value and get moved are usually intentional
int move_ok(std::set<int> source) {
  pass_rvalue_ref(std::move(source));
  return 0;
}

int param_ref_move_ok(std::set<int> source) {
  auto& source_ref = source;
  pass_rvalue_ref(std::move(source_ref));
  return 0;
}

int static_cast_to_rvalue_ref_ok(std::set<int> source) {
  pass_rvalue_ref(static_cast<std::set<int>&&>(source));
  return 0;
}

// structs known to be cheap to copy are not reported
int std_pair_int_ok(std::pair<int, int> p) { return p.first; }

int std_pair_vector_bad(std::pair<int, std::vector<int>> p) { return p.first; }

std::string std_pair_string_bad(std::pair<std::string, std::string> p) {
  return p.first;
}

int folly_optional_int_ok(folly::Optional<int> n_opt) {
  if (n_opt.has_value()) {
    return 42;
  }
  return 0;
}

int folly_optional_vector_bad(folly::Optional<std::vector<int>> vec_opt) {
  if (vec_opt.has_value()) {
    return 42;
  }
  return 0;
}

int folly_optional_string_bad(folly::Optional<std::string> s_opt) {
  if (s_opt.has_value()) {
    return 42;
  }
  return 0;
}

bool folly_optional_string_access_bad(folly::Optional<std::string> s_opt) {
  return s_opt->empty();
}

bool is_empty_string(const folly::Optional<std::string>& s_opt) {
  return s_opt->empty();
}

bool folly_optional_string_access_in_callee_bad(
    folly::Optional<std::string> s_opt) {
  return is_empty_string(s_opt);
}

struct StructWithInt {
  int n;
  std::vector<int> vec;
};

void havoc_ptr(int* p);

// StructWithInt s is const refable since it is not modified
void havoc_reachable_by_unknown_bad(int* p, StructWithInt s) {
  if (s.n == 42 && *p == 42) {
    havoc_ptr(p);
  }
}

void dead_param_ok(folly::Optional<std::string> s_opt) {}

void call_lambda(const std::function<void()>& f) { f(); }

// This is FN in c++11.
void captured_arr_bad_FN(Arr a) {
  call_lambda([&a]() {});
}

// This is TN in c++11.
void captured_arr_ok(Arr a) {
  call_lambda([&a]() { a.arr[0] += 8; });
}

// This is TP in c++11.
void captured_shared_ptr_bad_FN(std::shared_ptr<int> a) {
  call_lambda([&a]() {});
}

// lock() does not modify the weak pointer, nor does writing to the locked
// object
void lock_weak_ptr_bad(std::weak_ptr<int> p) {
  if (auto s = p.lock()) {
    *s = 1;
  }
}

int reset_weak_ptr_ok(std::weak_ptr<int> p) {
  auto s = p.lock();
  p.reset();
  return s ? *s : 0;
}

Arr global;

class AssignField {
  Arr field;

 public:
  // It should NOT report const refable issue, but unncessary copy assignment
  // issue.
  void assign_field_bad(Arr a) { field = a; }

  // It should NOT report const refable issue, but unncessary copy assignment
  // issue.
  void assign_global_bad(Arr a) { global = a; }
};

// Suppress const refable issues on functions returning folly::coro::Task
folly::coro::Task<Arr> ret_coro_task_ok(std::string s) {
  int n = s.length();
  return folly::coro::Task<Arr>();
}

void move_iterated_vector_ok(std::vector<std::string> v) {
  std::vector<std::string> local;
  local.insert(local.end(),
               std::make_move_iterator(v.begin()),
               std::make_move_iterator(v.end()));
}

// The same adaptor, spelled by naming its constructor instead of calling the helper. Taking
// the parameter by const reference would turn the moves back into copies, so neither spelling
// should be advised to do so.
void move_iterated_vector_ctor_ok(std::vector<std::string> v) {
  std::vector<std::string> local(
      std::move_iterator<std::vector<std::string>::iterator>(v.begin()),
      std::move_iterator<std::vector<std::string>::iterator>(v.end()));
}

// The control: the same shape without the adaptor copies the elements, so the parameter really
// could be taken by const reference.
void copy_iterated_vector_bad(std::vector<std::string> v) {
  std::vector<std::string> local(v.begin(), v.end());
}

char mod_char(char);

void modify_string(std::string& s) {
  for (char& c : s) {
    c = mod_char(c);
  }
}

void call_modify_string_ok(std::string s) { modify_string(s); }

void void_cast(std::string* s) { (void)s; }

// FN because now infer_skip is addressed as an unknown call
void call_void_cast_bad(std::string s) { void_cast(&s); }

void read_chars(const char* p);

void read_c_str_bad(std::string s) { read_chars(s.c_str()); }

// [(void)p] is translated to an unknown call that is considered to write to
// what [p] points to
void void_cast_c_str_bad(std::string s) {
  const char* p = s.c_str();
  (void)p;
}

int get_lambda(const std::function<int(Arr)>& f, Arr a) {
  return f(std::move(a));
}

int call_get_lambda_ok(Arr a) {
  return get_lambda([](Arr a) { return a.vec[0]; }, std::move(a));
}

std::string use_unique_ptr_ok(std::unique_ptr<std::string> x) {
  return *x.get();
}

struct NonCopiableT {
  std::vector<int> vec;
  int x;
  // doesn't allow copying
  NonCopiableT(const NonCopiableT&) = delete;
};

// we shouldn't report const-refable here since the type doesn't allow copies
void non_copiable_ok(NonCopiableT t) { auto p = t.x; }

void modify_string_ok(std::string s) {
  char* p = &s[0];
  *p = 'a';
}

// currently the model semantics cannot follow the pointer arithmetics precisely
void modify_string2_ok_FP(std::string s) {
  char* p = &s[3];
  *p = 'a';
}

class N {
  uint32_t n;
};
using TupleN = std::tuple<N, N>;

void use_tupleN(TupleN x);

void pass_tupleN_ok(TupleN x) { use_tupleN(x); }

std::string move_to_return_ok(bool b, std::string s) {
  if (b) {
    return "hi";
  } else {
    return s; // s is moved here.
  }
}

void unknown_lambda_call(std::function<void()>);

// The issue is conservatively suppressed when the parameter is captured by
// reference.
void captured_ref_ok(std::vector<int> vec) {
  auto f = [&vec]() { vec[0] += 42; };
  unknown_lambda_call(std::move(f));
}

void captured_value_bad(std::vector<int> vec) {
  auto f = [vec]() {};
  unknown_lambda_call(std::move(f));
}

void captured_move_ok(std::vector<int> vec) {
  auto f = [vec = std::move(vec)]() {};
  unknown_lambda_call(std::move(f));
}

// A method that overrides a virtual one cannot change its parameter types: the
// declaration it overrides fixes them, and taking the parameter by const
// reference would mean changing the base and every other implementation and
// caller too. The copy is real but the advice cannot be taken here, so these are
// not reported. A virtual method that overrides nothing still is: whoever wrote
// that declaration owns its signature.
namespace virtual_override {

void read_int(int);
void read_size(unsigned long);

struct Interface {
  virtual ~Interface() = default;
  virtual void record(std::shared_ptr<int> p) = 0;
  virtual void tally(std::vector<int> v) = 0;
};

struct Impl : Interface {
  void record(std::shared_ptr<int> p) override { read_int(*p); }
  void tally(std::vector<int> v) override { read_size(v.size()); }
};

// Overriding an override is still overriding.
struct Deeper : Impl {
  void record(std::shared_ptr<int> p) override { read_int(*p + 1); }
};

struct Root {
  virtual ~Root() = default;
  // Virtual, but overrides nothing, so the signature is this declaration's to
  // change.
  virtual void root_record_bad(std::shared_ptr<int> p) { read_int(*p); }
};

struct Plain {
  void plain_record_bad(std::shared_ptr<int> p) { read_int(*p); }
};

struct Overload : Interface {
  void record(std::shared_ptr<int> p) override { read_int(*p); }
  void tally(std::vector<int> v) override { read_size(v.size()); }
  // Reported: it only shares a name with the method it inherits, and an
  // overload chooses its own parameters. Deliberately not named with a _bad
  // suffix -- the name has to be the inherited one for this to be a control
  // that tells matching on the signature apart from matching on the name. The
  // parameter is called `q` so that the expected output names which of the two
  // `record` methods is the one still reported; both render the same otherwise.
  void record(std::shared_ptr<int> q, int extra) { read_int(*q + extra); }
};

// Known limitation. A derived class may declare an INSTANCE method with the
// same name and the same parameter types as a NON-virtual base method. That
// hides the base method instead of overriding it, so this declaration does own
// its signature and the advice is actionable here. The check cannot tell the
// two apart: it asks whether a supertype declares the same signature, and
// nothing records whether a method is virtual -- the clang frontend reads it
// but drops it before the IR -- so the report is lost here too. A
// virtuality-aware check would restore the Hider, DeepHider and CrtpDerived
// rows below. The limitation stops at instance methods: a static one is
// recognised, since a static can never override.
struct NonVirtualBase {
  void hides_record_bad(std::shared_ptr<int> p) { read_int(*p); }
};

struct Hider : NonVirtualBase {
  // Same name and signature as the base, which is not virtual, so this hides it
  // rather than overriding it. Not reported, and it should be.
  void hides_record_bad(std::shared_ptr<int> p) { read_int(*p + 1); }
};

// Hiding a hider is still hiding, and is lost the same way.
struct DeepHider : Hider {
  void hides_record_bad(std::shared_ptr<int> p) { read_int(*p + 2); }
};

// A STATIC member function cannot override anything: clang rejects a static
// that tries to. It must carry the INHERITED name to be a test at all, since
// the check matches a name together with exact parameter types; spelled any
// other way it hides nothing, the check never looks at it, and the row would be
// reported whether or not a static is excluded. With the name it does hide the
// base method, so this declaration owns its signature and is reported, and the
// row disappears if the static exclusion is reverted.
struct StaticHider : NonVirtualBase {
  static void hides_record_bad(std::shared_ptr<int> p) { read_int(*p + 3); }
};

// The curiously recurring template pattern reaches the derived class without
// virtual functions, so the derived declaration owns its signature too, and is
// lost to the same limitation as Hider. The base is reported; the explicit
// instantiation below is what makes its body exist to be analysed.
template <class Derived>
struct CrtpBase {
  void crtp_record_bad(std::shared_ptr<int> p) { read_int(*p); }
};

struct CrtpDerived : CrtpBase<CrtpDerived> {
  void crtp_record_bad(std::shared_ptr<int> p) { read_int(*p + 1); }
};

template struct CrtpBase<CrtpDerived>;

} // namespace virtual_override
} // namespace const_refable
