/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Minimal boost::program_options caricature for testing the adoption model. It mirrors the
// real header: value<T>() hands over a raw new-ed typed_value<T>*, and the option-declaration
// chain adopts it into a shared_ptr<const value_semantic> through a parameter that is a
// POINTER TO CONST. The chain's operator() has no body here, exactly as in the real library,
// where it is compiled into libboost_program_options.

namespace boost {
namespace program_options {

class value_semantic {
 public:
  virtual ~value_semantic() {}
};

template <class T>
class typed_value : public value_semantic {
 public:
  typed_value* default_value(const T& v) { return this; }
  typed_value* required() { return this; }
};

template <class T>
typed_value<T>* value() {
  return new typed_value<T>();
}

class options_description_easy_init {
 public:
  // the three real overloads; none has a body, as in the library
  options_description_easy_init& operator()(const char* name,
                                            const char* description);
  options_description_easy_init& operator()(const char* name,
                                            const value_semantic* s);
  options_description_easy_init& operator()(const char* name,
                                            const value_semantic* s,
                                            const char* description);
};

class option_description {
 public:
  option_description(const char* name, const value_semantic* s);
  option_description(const char* name,
                     const value_semantic* s,
                     const char* description);
};

class options_description {
 public:
  options_description_easy_init add_options();
};

} // namespace program_options
} // namespace boost

namespace po = boost::program_options;

namespace program_options_adoption {

// ------------------------------ the FP this removes ------------------------------

void declare_one_option_ok() {
  po::options_description desc;
  desc.add_options()("mode", po::value<int>(), "the mode");
}

void declare_option_two_arg_overload_ok() {
  po::options_description desc;
  desc.add_options()("mode", po::value<int>());
}

// the chained shape, which is how almost all real call sites look
void declare_chained_options_ok() {
  po::options_description desc;
  desc.add_options()("mode", po::value<int>(), "the mode")(
      "file", po::value<int>(), "the file")("verbose", "be loud");
}

void declare_with_default_value_ok() {
  po::options_description desc;
  desc.add_options()("mode", po::value<int>()->default_value(3), "the mode");
}

// built by hand instead of through the chain
void option_description_directly_ok() {
  po::option_description d("mode", po::value<int>(), "the mode");
}

// ------------------------------- TRUE POSITIVE CONTROLS -------------------------------

// A value_semantic that is never handed to boost at all is still leaked by us. This is the
// control that the model is keyed on the ADOPTING CALL and not on the allocating one.
void value_never_adopted_bad() { po::value<int>(); }

// The (const char*, const char*) overload must NOT be modelled as adopting: its second
// argument is a description, not a value. Here a heap pointer is passed as that description
// and we still own it.
void description_overload_does_not_adopt_bad() {
  po::options_description desc;
  desc.add_options()("mode", new char[8]);
}

struct Thing {
  int v;
};

// An unrelated callee with no body taking a pointer to const does NOT adopt. This is the
// control that the model did not become a blanket "const T* escapes" rule.
void unrelated_const_sink(const Thing* t);

void const_ptr_to_other_unknown_bad() { unrelated_const_sink(new Thing()); }

// The real-code shape this model deliberately keeps reporting: a config struct holding an
// owned raw pointer, handed to an unknown callee as a pointer to const. The callee may only
// be borrowing it -- as atval::create does -- so the leak stands.
struct Config {
  Thing* owned;
};

void unknown_create(const Config* cfg);

void config_with_owned_pointer_bad() {
  Config cfg{};
  cfg.owned = new Thing();
  unknown_create(&cfg);
}

} // namespace program_options_adoption
