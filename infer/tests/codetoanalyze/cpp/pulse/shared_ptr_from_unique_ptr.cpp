/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstdio>
#include <memory>

namespace shared_ptr_from_unique_ptr {

std::unique_ptr<int> make_int(int i) {
  return std::unique_ptr<int>(new int(i));
}

struct Holder {
  std::shared_ptr<int> p;
  void init() { p = make_int(1); }
  int& get() {
    init();
    return *p;
  }
};

int& assigned_from_unique_ptr_ok(Holder& h) { return h.get(); }

int& constructed_from_unique_ptr_ok(std::shared_ptr<int>& s) {
  s = std::shared_ptr<int>(make_int(1));
  return *s;
}

void no_leak_ok() { std::shared_ptr<int> s(std::unique_ptr<int>(new int(1))); }

void moved_from_unique_ptr_null_deref_bad() {
  std::unique_ptr<int> u(new int(1));
  std::shared_ptr<int> s(std::move(u));
  *u = 2;
}

void use_after_last_owner_bad() {
  std::unique_ptr<int> u(new int(1));
  int* raw = u.get();
  {
    std::shared_ptr<int> s(std::move(u));
  }
  *raw = 2;
}

void use_after_assigned_last_owner_bad() {
  std::unique_ptr<int> u(new int(1));
  int* raw = u.get();
  {
    std::shared_ptr<int> s;
    s = std::move(u);
  }
  *raw = 2;
}

void assignment_releases_old_object_bad() {
  std::shared_ptr<int> s(new int(1));
  int* old = s.get();
  s = make_int(2);
  *old = 3;
}

struct Base {
  virtual ~Base() = default;
  int v;
};

struct Owner : Base {
  std::unique_ptr<Base> child;
};

// the unique_ptr is emptied before the old object that contains it is released
int assign_from_field_of_released_object_ok() {
  Owner* owner = new Owner();
  owner->child.reset(new Base());
  owner->child->v = 3;
  std::shared_ptr<Base> s(owner);
  s = std::move(owner->child);
  return s->v;
}

int assign_from_field_then_use_old_object_bad() {
  Owner* owner = new Owner();
  owner->child.reset(new Base());
  std::shared_ptr<Base> s(owner);
  s = std::move(owner->child);
  return owner->v;
}

void function_pointer_deleter_ok(const char* path) {
  if (FILE* file = fopen(path, "r")) {
    std::unique_ptr<FILE, int (*)(FILE*)> u(file, fclose);
    std::shared_ptr<FILE> s(std::move(u));
  }
}

void function_pointer_deleter_close_twice_bad(const char* path) {
  if (FILE* file = fopen(path, "r")) {
    std::unique_ptr<FILE, int (*)(FILE*)> u(file, fclose);
    std::shared_ptr<FILE> s(std::move(u));
    fclose(s.get());
  }
}

struct FileCloser {
  void operator()(FILE* f) const { fclose(f); }
};

void functor_deleter_close_twice_bad(const char* path) {
  if (FILE* file = fopen(path, "r")) {
    std::unique_ptr<FILE, FileCloser> u(file);
    std::shared_ptr<FILE> s(std::move(u));
    fclose(s.get());
  }
}

// unlike shared_ptr(p, d), an empty unique_ptr gives an empty shared_ptr that
// does not call the deleter
void empty_unique_ptr_ok() {
  std::unique_ptr<FILE, int (*)(FILE*)> u(nullptr, fclose);
  std::shared_ptr<FILE> s(std::move(u));
}

int& stack_address_bad(Holder& h) {
  int x = 0;
  h.init();
  return x;
}

} // namespace shared_ptr_from_unique_ptr
