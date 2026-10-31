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

namespace container_owning_key {

struct Box {
  char* p;
};

// `const Key` makes the key object const, not what it points to, so the key of
// a map-like container can be the owner. Each container here outlives the call.
void map_emplace_key_ok(std::map<Box*, int>& out) {
  out.emplace((Box*)malloc(16), 1);
}

void map_insert_pair_key_ok(std::map<Box*, int>& out) {
  out.insert({(Box*)malloc(16), 1});
}

void unordered_map_emplace_key_ok(std::unordered_map<Box*, int>& out) {
  out.emplace((Box*)malloc(16), 1);
}

void multimap_emplace_key_ok(std::multimap<Box*, int>& out) {
  out.emplace((Box*)malloc(16), 1);
}

// Both halves own, and both have to stay reachable.
void map_emplace_both_ok(std::map<Box*, Box*>& out) {
  out.emplace((Box*)malloc(16), (Box*)malloc(16));
}

// Interning: the allocation is the key and the map keeps it alive.
void intern_ok(std::map<char*, int>& table) {
  char* key = (char*)malloc(32);
  table.emplace(key, 1);
}

#if __cplusplus >= 201703L
void map_try_emplace_key_ok(std::map<Box*, int>& out) {
  out.try_emplace((Box*)malloc(16), 1);
}

void map_insert_or_assign_key_ok(std::map<Box*, int>& out) {
  out.insert_or_assign((Box*)malloc(16), 1);
}

void local_map_try_emplace_key_bad() {
  std::map<Box*, int> m;
  m.try_emplace((Box*)malloc(16), 1);
}
#endif // __cplusplus >= 201703L

// TRUE POSITIVES: relating the key to the container does not save it when the
// container is local and dies.
void local_map_emplace_key_bad() {
  std::map<Box*, int> m;
  m.emplace((Box*)malloc(16), 1);
}

void local_map_insert_pair_key_bad() {
  std::map<Box*, int> m;
  m.insert({(Box*)malloc(16), 1});
}

void local_map_emplace_both_bad() {
  std::map<Box*, Box*> m;
  m.emplace((Box*)malloc(16), (Box*)malloc(16));
}

// A set's element is the owner directly: the control that already worked.
void set_insert_ok(std::set<Box*>& out) { out.insert((Box*)malloc(16)); }

void plain_leak_bad() { malloc(16); }

} // namespace container_owning_key
