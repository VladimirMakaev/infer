/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstdlib>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace container_emplace {

struct Box {
  char* p;
};

// An owner handed to a container that outlives the call is not a leak.
void emplace_ok(std::set<Box*>& s) { s.emplace((Box*)malloc(16)); }

void emplace_hint_ok(std::set<Box*>& s) {
  s.emplace_hint(s.begin(), (Box*)malloc(16));
}

void unordered_emplace_ok(std::unordered_set<Box*>& s) {
  s.emplace((Box*)malloc(16));
}

// The map-like family builds the entry in place rather than taking a pair, so
// each of these has to be recognised too.
#if __cplusplus >= 201703L
void map_try_emplace_ok(std::map<int, Box*>& m) {
  m.try_emplace(1, (Box*)malloc(16));
}

void map_insert_or_assign_ok(std::map<int, Box*>& m) {
  m.insert_or_assign(1, (Box*)malloc(16));
}

void local_map_try_emplace_bad() {
  std::map<int, Box*> m;
  m.try_emplace(1, (Box*)malloc(16));
}

void local_map_insert_or_assign_bad() {
  std::map<int, Box*> m;
  m.insert_or_assign(1, (Box*)malloc(16));
}
#endif // __cplusplus >= 201703L

void map_emplace_ok(std::map<int, Box*>& m) { m.emplace(1, (Box*)malloc(16)); }

// `emplace_hint` takes the hint first and the entry after it; the hint is an
// iterator and owns nothing. Either half of the entry can be the owner.
void map_emplace_hint_ok(std::map<int, Box*>& m) {
  m.emplace_hint(m.begin(), 1, (Box*)malloc(16));
}

void map_emplace_hint_key_ok(std::map<Box*, int>& m) {
  m.emplace_hint(m.begin(), (Box*)malloc(16), 1);
}

void map_emplace_hint_both_ok(std::map<Box*, Box*>& m) {
  m.emplace_hint(m.begin(), (Box*)malloc(16), (Box*)malloc(16));
}

void unordered_map_emplace_hint_ok(std::unordered_map<int, Box*>& m) {
  m.emplace_hint(m.begin(), 1, (Box*)malloc(16));
}

void multimap_emplace_hint_ok(std::multimap<int, Box*>& m) {
  m.emplace_hint(m.begin(), 1, (Box*)malloc(16));
}

// TRUE POSITIVES: the container is local, so what it owns dies with it.
void local_emplace_bad() {
  std::set<Box*> s;
  s.emplace((Box*)malloc(16));
}

void local_unordered_emplace_bad() {
  std::unordered_set<Box*> s;
  s.emplace((Box*)malloc(16));
}

void local_map_emplace_bad() {
  std::map<int, Box*> m;
  m.emplace(1, (Box*)malloc(16));
}

void local_map_emplace_hint_bad() {
  std::map<int, Box*> m;
  m.emplace_hint(m.begin(), 1, (Box*)malloc(16));
}

void local_map_emplace_hint_key_bad() {
  std::map<Box*, int> m;
  m.emplace_hint(m.begin(), (Box*)malloc(16), 1);
}

void local_map_emplace_hint_both_bad() {
  std::map<Box*, Box*> m;
  m.emplace_hint(m.begin(), (Box*)malloc(16), (Box*)malloc(16));
}

void local_multimap_emplace_hint_bad() {
  std::multimap<int, Box*> m;
  m.emplace_hint(m.begin(), 1, (Box*)malloc(16));
}

// The size is tracked, so the container is no longer empty after an emplace.
bool emplace_then_empty_ok() {
  std::set<int> s;
  s.emplace(1);
  return s.empty();
}

void plain_leak_bad() { malloc(16); }

} // namespace container_emplace
