/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

namespace invalid_access_path {

struct Thing {
  int flag;
  bool enabled() const { return flag != 0; }
};

Thing* make_thing();

Thing* lookup(bool found) {
  if (found) {
    return make_thing();
  }
  return nullptr;
}

struct Holder {
  Thing* cached_ = nullptr;
  bool reserve(Thing* t) { return t->enabled(); }
  Thing* get() const { return cached_; }
};

template <class T>
T* wrap(Holder& h, Thing* t) {
  if (h.reserve(t)) {
    return reinterpret_cast<T*>(h.get());
  }
  return nullptr;
}

// One dereference of the null parameter t. The later read of cached_ must not
// rename that access on another disjunct: expect one report naming t.
void one_defect_reported_twice_bad() {
  Holder h;
  Thing* t = lookup(false);
  if (auto* v = wrap<int>(h, t)) {
    *v = 1;
  }
}

// Separate branches reach different dereferences. Both reports must remain.
void two_distinct_defects_bad(bool c) {
  if (c) {
    Thing* a = lookup(false);
    a->enabled();
  } else {
    Thing* b = lookup(false);
    b->enabled();
  }
}

void guarded_ok() {
  Holder h;
  Thing* t = make_thing();
  if (t != nullptr) {
    wrap<int>(h, t);
  }
}

// The write accesses used, even though cached_ holds the same null value.
void null_store_bad() {
  Thing* used = lookup(false);
  Holder h;
  (void)h.get();
  used->flag = 1;
}

// Naming changes must retain a genuine invalid-access control.
int use_after_delete_bad() {
  Thing* used = new Thing{};
  delete used;
  return used->flag;
}

int overwrite_pointer(Thing* t, bool overwrite) {
  if (overwrite) {
    t = nullptr;
  }
  return t->flag;
}

// The callee overwrites its parameter. The report must not blame the nonnull
// caller argument live for that new null value.
// Known limitation: this caller has no report on the baseline either.
int FN_reassigned_parameter_bad() {
  Thing live{1};
  return overwrite_pointer(&live, true);
}

int array_access(Thing** p, int i) { return p[i]->flag; }

// Both the pointer and the index are actual arguments: any rebased access path
// must use items[index], not a caller base paired with the callee's index i.
int indexed_parameter_bad() {
  Thing* items[] = {nullptr};
  int index = 0;
  return array_access(items, index);
}

int change_index(Thing** p, int i, bool change) {
  if (p[i] == nullptr && change) {
    ++i;
    p[i] = nullptr;
    return p[i]->flag;
  }
  return 0;
}

// The old and new indexed values are both null, but the index was changed in
// the callee. Keep the callee's path rather than naming the old caller index.
// Known limitation: this caller has no report on the baseline either.
int FN_reassigned_index_bad() {
  Thing* items[] = {nullptr, nullptr};
  int index = 0;
  return change_index(items, index, true);
}

int dereference(Thing* t) { return t->flag; }

// A literal null argument cannot borrow the name of another null-valued field.
int literal_argument_bad() {
  Holder h;
  (void)h.get();
  return dereference(nullptr);
}

} // namespace invalid_access_path
