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

// Writes through character offsets mutate the string parameter.
void modify_string2_ok(std::string s) {
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
// that declaration owns its signature. Only a genuine override is suppressed --
// hiding a non-virtual base method is not overriding, and neither is a static.
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

// Hiding is not overriding. A derived class may declare a method with the same
// name and the same parameter types as a NON-virtual base method; that hides
// the base method rather than overriding it, so the declaration owns its
// signature and the advice is actionable. The check asks whether the method it
// found in a supertype is virtual, so all of these stay reported.
struct NonVirtualBase {
  void hides_record_bad(std::shared_ptr<int> p) { read_int(*p); }
};

struct Hider : NonVirtualBase {
  void hides_record_bad(std::shared_ptr<int> p) { read_int(*p + 1); }
};

// Hiding a hider is still hiding.
struct DeepHider : Hider {
  void hides_record_bad(std::shared_ptr<int> p) { read_int(*p + 2); }
};

// The case that separates the two questions one could ask. This method IS
// declared virtual, but the base method it shares a signature with is not, so
// it overrides nothing: it starts a new hierarchy and chooses its own
// parameters. Asking "is this method virtual" would wrongly suppress it;
// asking "is the method it overrides virtual" reports it.
struct VirtualHider : NonVirtualBase {
  virtual void hides_record_bad(std::shared_ptr<int> p) { read_int(*p + 4); }
};

// A derived class can reach a same-signature method through SEVERAL bases, and
// only one of them need be virtual for this to be a real override. The check
// keeps looking after a base whose method is not virtual, so neither listing
// order changes the answer: both of these override VirtualRecorder and both are
// suppressed. They are here because a search that stopped at the first
// same-signature base would still pass every single-base case above, and would
// report whichever of the two listed the non-virtual base first.
struct VirtualRecorder {
  virtual ~VirtualRecorder() = default;
  // Pure virtual on purpose: a declaration with no body is not analysed and so
  // adds no expected row, while still carrying the virtuality the check reads.
  virtual void hides_record_bad(std::shared_ptr<int> p) = 0;
};

struct MixedNonVirtFirst : NonVirtualBase, VirtualRecorder {
  void hides_record_bad(std::shared_ptr<int> p) override { read_int(*p + 5); }
};

struct MixedVirtFirst : VirtualRecorder, NonVirtualBase {
  void hides_record_bad(std::shared_ptr<int> p) override { read_int(*p + 6); }
};

// A STATIC member function cannot override anything: clang rejects a static
// that tries to. It must carry the INHERITED name to be a test at all, since
// the check matches a name together with exact parameter types; spelled any
// other way it hides nothing, the check never looks at it, and the row would be
// reported whether or not a static is excluded. With the name it does hide the
// base method, so this declaration owns its signature and is reported.
//
// This row no longer pins the separate static exclusion in the check, and a
// reader should not assume it does. Clang rejects a static that shares a
// signature with a VIRTUAL base method outright, so any static that reaches the
// override search has only non-virtual same-signature bases -- and those are
// reported anyway, on the strength of the virtuality test. Removing the static
// exclusion leaves this row, and every expectation in this suite, unchanged.
//
// That is measured rather than argued, and it is still not permission to drop
// the exclusion. It is spelled as "the method kind is CPP_INSTANCE", which also
// excludes Objective-C, whose methods are OBJC_INSTANCE or OBJC_CLASS. There is
// no Objective-C case in this suite, so nothing here can observe that half of
// it in either direction. A green run says the C++ side is unaffected; it says
// nothing at all about the rest.
struct StaticHider : NonVirtualBase {
  static void hides_record_bad(std::shared_ptr<int> p) { read_int(*p + 3); }
};

// The curiously recurring template pattern reaches the derived class without
// virtual functions, so the derived declaration owns its signature too, and is
// reported along with the base. The explicit instantiation below is what makes
// the base's body exist to be analysed.
template <class Derived>
struct CrtpBase {
  void crtp_record_bad(std::shared_ptr<int> p) { read_int(*p); }
};

struct CrtpDerived : CrtpBase<CrtpDerived> {
  void crtp_record_bad(std::shared_ptr<int> p) { read_int(*p + 1); }
};

template struct CrtpBase<CrtpDerived>;

// Sharing a name with a virtual base method is not overriding it either: the
// parameter TYPES have to match. They are compared through the overridden
// method's formals rather than through its procname, because
// `Procname.Parameter.clang_parameter` records a name only for a pointer to a
// struct and maps every other parameter to `None`. By that measure a by-value
// parameter carries no type at all, and the test degenerates into the method
// name and the parameter COUNT -- which silences an overload that merely
// changed a by-value type, and silences it on nothing more than an arity
// coincidence when the base parameter is not even a class type.
//
// Each derived method below deliberately keeps the inherited name, since the
// search matches on the name first; spelled any other way the check would never
// look at it and the row would be reported either way. Parameter names differ
// from the base's so the expected output says which method each row is.
namespace byvalue_signature {

// A by-value parameter type that merely differs. None of these overrides
// anything, so each owns its signature and is reported.
struct ParamBase {
  virtual ~ParamBase() = default;
  // Pure virtual throughout: a declaration with no body adds no expected row
  // while still carrying the formals the check reads.
  virtual void record(std::shared_ptr<int> p) = 0;
  virtual void tally(std::shared_ptr<int> p) = 0;
  // The base parameter is not a class type at all. Nothing but the arity is
  // shared, which is the weakest form of the coincidence.
  virtual void count(int n) = 0;
  virtual void measure(double d) = 0;
};

struct ParamDerived : ParamBase {
  // `const` makes it a different type, so this overloads rather than overrides.
  void record(std::shared_ptr<const int> q) { read_int(*q); }
  // A different element type, likewise an overload.
  void tally(std::shared_ptr<long> r) { read_int(static_cast<int>(*r)); }
  // Matches `count` on arity alone.
  void count(std::shared_ptr<int> s) { read_int(*s); }
  // The same coincidence reaching PULSE_CONST_REFABLE rather than the
  // shared_ptr flavour, so one fix is not mistaken for covering one checker.
  void measure(std::string t) { read_size(t.size()); }
};

// REAL overrides from here down. Every one of them must stay suppressed: a
// comparison that is too strict turns each into a false positive, which is
// worse than the false negatives above. They cover the ways two spellings of
// one parameter type can look different before the frontend is done with them,
// and the ways a receiver can differ.

// An alias in the base and the type spelled out in the derived, and the
// reverse. The frontend desugars both, so the formals match.
typedef std::shared_ptr<int> IntPtrTypedef;
using IntPtrAlias = std::shared_ptr<int>;

struct AliasBase {
  virtual ~AliasBase() = default;
  virtual void record(IntPtrAlias p) = 0;
};

struct AliasDerived : AliasBase {
  void record(std::shared_ptr<int> p) override { read_int(*p); }
};

struct TypedefBase {
  virtual ~TypedefBase() = default;
  virtual void record(std::shared_ptr<int> p) = 0;
};

struct TypedefDerived : TypedefBase {
  void record(IntPtrTypedef p) override { read_int(*p + 1); }
};

// A template base: the override lives in one instantiation, and the parameter
// type is instantiated before it is recorded.
template <class T>
struct TemplateBase {
  virtual ~TemplateBase() = default;
  virtual void record(std::shared_ptr<T> p) = 0;
};

struct TemplateDerived : TemplateBase<int> {
  void record(std::shared_ptr<int> p) override { read_int(*p + 2); }
};

// A default argument on the base only. Defaults are not part of the type, so
// the two still match.
struct DefaultArgBase {
  virtual ~DefaultArgBase() = default;
  virtual void record(std::shared_ptr<int> p, int n = 0) = 0;
};

struct DefaultArgDerived : DefaultArgBase {
  void record(std::shared_ptr<int> p, int n) override { read_int(*p + n); }
};

// `const` member functions: the receiver differs in BOTH its class and its
// constness. Dropping the leading formal is what keeps this pair matching.
struct ConstMethodBase {
  virtual ~ConstMethodBase() = default;
  virtual void record(std::shared_ptr<int> p) const = 0;
};

struct ConstMethodDerived : ConstMethodBase {
  void record(std::shared_ptr<int> p) const override { read_int(*p + 3); }
};

// Only the parameter NAMES differ. Formals carry names as well as types, so a
// comparison that comes to include them would break here and nowhere else.
struct RenameBase {
  virtual ~RenameBase() = default;
  virtual void record(std::shared_ptr<int> first) = 0;
};

struct RenameDerived : RenameBase {
  void record(std::shared_ptr<int> second) override { read_int(*second + 4); }
};

// An override against a GRANDparent, with the class in between silent.
struct GrandBase {
  virtual ~GrandBase() = default;
  virtual void record(std::shared_ptr<int> p) = 0;
};

struct GrandMiddle : GrandBase {};

struct GrandDerived : GrandMiddle {
  void record(std::shared_ptr<int> p) override { read_int(*p + 5); }
};

// An override against the SECOND base, so the search has to get past a base
// that declares nothing of the kind.
struct UnrelatedBase {
  virtual ~UnrelatedBase() = default;
  virtual void unrelated() = 0;
};

struct SecondBase {
  virtual ~SecondBase() = default;
  virtual void record(std::shared_ptr<int> p) = 0;
};

struct SecondDerived : UnrelatedBase, SecondBase {
  void unrelated() override {}
  void record(std::shared_ptr<int> p) override { read_int(*p + 6); }
};

// Controls, so a run that suppressed everything could not pass. Arity and a
// pointer-to-struct parameter are the two things the procname comparison DID
// get right, and they must keep working.
struct ArityBase {
  virtual ~ArityBase() = default;
  virtual void record(std::shared_ptr<int> p) = 0;
};

struct ArityDerived : ArityBase {
  // One parameter more than the base: already told apart before this change.
  void record(std::shared_ptr<int> p, int extra_bad) { read_int(*p + extra_bad); }
};

struct Pointee {
  int x;
};

struct PointerBase {
  virtual ~PointerBase() = default;
  // A pointer to a struct is the one parameter shape the procname DOES record,
  // so this pair was never confused.
  virtual void record(Pointee* p) = 0;
};

struct PointerDerived : PointerBase {
  void record(std::shared_ptr<Pointee> u) { read_int(u->x); }
};

// A top-level cv-qualifier on a parameter is not part of a C++ function type: void f(const T)
// and void f(T) declare the SAME function, and clang accepts an override that adds or drops
// one. Every cell down to the pointee control is therefore a REAL override and must stay
// silent -- comparing the types naively reports all of them, on nothing but a coding style.
//
// The frontend lowers a by-value class parameter to a reference, so `const Val p` arrives as
// `Val const &`: the qualifier sits on the pointee of a reference the source never wrote.
// That is why the base-side and derived-side cells below are not the same test as the scalar
// one, and why both are needed.

struct QualBase {
  virtual ~QualBase() = default;
  // const on the BASE side only
  virtual void base_const(const std::shared_ptr<int> p) = 0;
  // const on BOTH sides: agreed even before this, and here to keep the pair honest
  virtual void both_const(const std::shared_ptr<int> p) = 0;
  // a const SCALAR next to the reportable parameter: the qualifier is top-level on the formal
  // itself rather than on a lowered reference, which is a different code path
  virtual void scalar_const(const int n, std::shared_ptr<int> p) = 0;
  // a const POINTER -- top-level again, but on the one parameter shape of_typ records
  virtual void pointer_const(Pointee* const q, std::shared_ptr<int> p) = 0;
};

struct QualDerived : QualBase {
  void base_const(std::shared_ptr<int> p) override { read_int(*p); }
  void both_const(const std::shared_ptr<int> p) override { read_int(*p + 2); }
  void scalar_const(int n, std::shared_ptr<int> p) override { read_int(n + *p); }
  void pointer_const(Pointee* q, std::shared_ptr<int> p) override { read_int(q->x + *p); }
};

// The mirror of base_const: the qualifier on the DERIVED side instead.
struct DerivedConstBase {
  virtual ~DerivedConstBase() = default;
  virtual void derived_const(std::shared_ptr<int> p) = 0;
};

struct DerivedConstDerived : DerivedConstBase {
  void derived_const(const std::shared_ptr<int> p) override { read_int(*p + 3); }
};

// THE CONTROL that stops the rule from being applied too widely. Here the qualifier is on the
// POINTEE of a real pointer, not of a lowered reference: `const Pointee*` and `Pointee*` are
// different parameter types, clang rejects an override between them, and this declaration
// owns its signature. It must keep reporting, which is what rules out comparing types with
// Typ.equal_ignore_quals -- that recurses through pointers and would silence this.
struct PointeeQualBase {
  virtual ~PointeeQualBase() = default;
  virtual void record(Pointee* q, std::shared_ptr<int> p) = 0;
};

struct PointeeQualDerived : PointeeQualBase {
  void record(const Pointee* q, std::shared_ptr<int> w) { read_int(q->x + *w); }
};

} // namespace byvalue_signature
} // namespace virtual_override
} // namespace const_refable
