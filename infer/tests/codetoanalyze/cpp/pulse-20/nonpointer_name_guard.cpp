/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <memory>

namespace nonpointer_name_guard {
namespace counter_field {

struct Thing {
  int flag;
  bool enabled() const { return flag != 0; }
};
Thing* make_thing();
bool use_thing(Thing* t) { return t->enabled(); }

// The null is created INSIDE the lookup, on a branch over the caller's state --
// as getTypeObject returns nullptr when the entry is absent.
struct Provider {
  int count_ = 0;
  Thing* lookup() const {
    if (count_ == 0) { return nullptr; }
    return make_thing();
  }
};

struct Remote {
  bool saved_ = false;   // a bool, never a pointer
  void write(Provider* p) { use_thing(p->lookup()); }
};

void serialize_bad() {
  Provider p;
  auto r = std::make_unique<Remote>();
  r->write(&p);
}

} // namespace counter_field
} // namespace nonpointer_name_guard

namespace nonpointer_name_guard {
namespace local_pointer {

struct Thing {
  int flag;
  bool enabled() const { return flag != 0; }
};
Thing* make_thing();
Thing* lookup(bool found) {
  if (found) { return make_thing(); }
  return nullptr;
}
bool use_thing(Thing* t) { return t->enabled(); }   // the real dereference

struct Remote {
  bool saved_ = false;    // a bool, initialised false, never a pointer
  void write() {
    Thing* t = lookup(false);
    use_thing(t);
  }
};

// The defect is the null `t` dereferenced in use_thing. The object made by make_unique
// merely has a false bool field, which shares the abstract value 0.
void serialize_bad() {
  auto r = std::make_unique<Remote>();
  r->write();
}

} // namespace local_pointer
} // namespace nonpointer_name_guard

namespace nonpointer_name_guard {
namespace pointer_field {

struct Thing {
  int flag;
  bool enabled() const { return flag != 0; }
};
bool use_thing(Thing* t) { return t->enabled(); }

// Mirrors the provider whose lookup yields null when the entry is absent.
struct Provider {
  Thing* slot_ = nullptr;
  Thing* get() const { return slot_; }
};

struct Remote {
  bool saved_ = false;   // a bool, never a pointer
  void write(Provider* p) {
    use_thing(p->get());   // latent here: depends on the caller's p->slot_
  }
};

// The ONE defect is the null returned by Provider::get, dereferenced in use_thing.
// `r->saved_` is a bool set to false by the constructor that make_unique invoked; it is
// never dereferenced, but it holds the same abstract value 0.
void serialize_bad() {
  Provider p;
  auto r = std::make_unique<Remote>();
  r->write(&p);
}

} // namespace pointer_field
} // namespace nonpointer_name_guard

namespace nonpointer_name_guard {

int direct_pointer_bad() {
  int* used = nullptr;
  return *used;
}

} // namespace nonpointer_name_guard
