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
#include <unordered_map>
#include <utility>

namespace container_emplace_owner {

// An owner the model cannot take apart: the container is handed the object, not
// the pointer it holds. The container outlives the call, so nothing is leaked.
void emplace_unique_ptr_into_map_ok(std::map<int, std::unique_ptr<int>>& m) {
  m.emplace(1, std::unique_ptr<int>(new int(2)));
}

void emplace_unique_ptr_into_unordered_map_ok(
    std::unordered_map<int, std::unique_ptr<int>>& m) {
  m.emplace(1, std::unique_ptr<int>(new int(2)));
}

void emplace_unique_ptr_into_set_ok(std::set<std::unique_ptr<int>>& s) {
  s.emplace(std::unique_ptr<int>(new int(2)));
}

void insert_unique_ptr_into_set_ok(std::set<std::unique_ptr<int>>& s) {
  s.insert(std::unique_ptr<int>(new int(2)));
}

void emplace_hint_unique_ptr_ok(std::set<std::unique_ptr<int>>& s) {
  s.emplace_hint(s.begin(), std::unique_ptr<int>(new int(2)));
}

#if __cplusplus >= 201703L
void try_emplace_unique_ptr_ok(std::map<int, std::unique_ptr<int>>& m) {
  m.try_emplace(1, std::unique_ptr<int>(new int(2)));
}

void insert_or_assign_unique_ptr_ok(std::map<int, std::unique_ptr<int>>& m) {
  m.insert_or_assign(1, std::unique_ptr<int>(new int(2)));
}
#endif // __cplusplus >= 201703L

// A user-defined owner, the shape of the reference wrappers that hold a pointer.
struct Ref {
  int* p;
  explicit Ref(int* p) : p(p) {}
  ~Ref() { delete p; }
  Ref(const Ref&) = delete;
  Ref(Ref&& o) : p(o.p) { o.p = nullptr; }
};

void emplace_owner_into_map_ok(std::map<int, Ref>& m) {
  m.emplace(1, Ref(new int(2)));
}

// The scalar cases keep working: what the container is handed is the pointer.
void emplace_pointer_into_map_ok(std::map<int, int*>& m) {
  m.emplace(1, new int(2));
}

// TRUE POSITIVES: the container is local, so what it holds dies with it, and
// nothing frees a raw pointer the container held.
void local_map_emplace_pointer_bad() {
  std::map<int, int*> m;
  m.emplace(1, new int(2));
}

void local_set_emplace_pointer_bad() {
  std::set<char*> s;
  s.emplace((char*)malloc(16));
}

} // namespace container_emplace_owner
