/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstdlib>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>

namespace container_element_destructor {

// An element that reclaims what it owns when it is destroyed. Destroying the
// container destroys every element, so nothing it holds is leaked.
struct Owner {
  int* p;
  Owner() : p(new int(0)) {}
  explicit Owner(int v) : p(new int(v)) {}
  ~Owner() { delete p; }
  Owner(const Owner& o) : p(new int(*o.p)) {}
  Owner& operator=(const Owner& o) {
    if (this != &o) {
      delete p;
      p = new int(*o.p);
    }
    return *this;
  }
  bool operator<(const Owner& o) const { return *p < *o.p; }
};

void local_map_of_owners_ok() {
  std::map<int, Owner> m;
  m[1] = Owner(2);
}

void local_unordered_map_of_owners_ok() {
  std::unordered_map<int, Owner> m;
  m[1] = Owner(2);
}

void local_map_of_unique_ptr_ok() {
  std::map<int, std::unique_ptr<int>> m;
  m.emplace(1, std::unique_ptr<int>(new int(2)));
}

void local_set_of_owners_ok() {
  std::set<std::string> s;
  s.insert(std::string("x"));
}

// The owner is reached through a pointer the element holds, the shape a JSON
// value with a union of pointers and a type tag has.
struct Tagged {
  Owner* owned;
  Tagged() : owned(nullptr) {}
  explicit Tagged(int v) : owned(new Owner(v)) {}
  ~Tagged() { delete owned; }
  Tagged(const Tagged& t) : owned(t.owned ? new Owner(*t.owned) : nullptr) {}
  Tagged& operator=(const Tagged& t) {
    if (this != &t) {
      delete owned;
      owned = t.owned ? new Owner(*t.owned) : nullptr;
    }
    return *this;
  }
};

void local_map_of_tagged_ok() {
  std::map<std::string, Tagged> m;
  m["a"] = Tagged(1);
}

// The key can be the owner too.
void local_map_owning_key_ok() {
  std::map<Owner, int> m;
  m.emplace(Owner(1), 2);
}

// The container is destroyed inside the destructor of whatever holds it, where
// none of its elements are known: the caller is where they are, so the effect
// has to reach it.
struct Holder {
  std::map<std::string, Tagged>* m;
  Holder() : m(new std::map<std::string, Tagged>()) {}
  ~Holder() { delete m; }
};

void map_held_by_a_wrapper_ok() {
  Holder h;
  (*h.m)["a"] = Tagged(1);
}

// TRUE POSITIVES: nothing destroys what these elements point at, so the
// container dying really does lose the only pointer to it.
void local_map_of_raw_bad() {
  std::map<int, int*> m;
  m.emplace(1, new int(1));
}

void local_set_of_raw_bad() {
  std::set<char*> s;
  s.insert((char*)malloc(16));
}

void local_map_raw_erased_bad() {
  std::map<int, int*> m;
  m.emplace(1, new int(1));
  m.erase(1);
}

// A string key does not make the raw mapped value anyone else's to free.
void local_map_string_key_raw_value_bad() {
  std::map<std::string, int*> m;
  m.emplace(std::string("a"), new int(1));
}

} // namespace container_element_destructor
