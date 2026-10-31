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
#include <vector>

namespace container_insert_ownership {

struct Box {
  char* p;
};

// Handing an owner to a container that outlives the call is not a leak.
void set_insert_ok(std::set<Box*>& s) { s.insert((Box*)malloc(16)); }

void unordered_set_insert_ok(std::unordered_set<Box*>& s) {
  s.insert((Box*)malloc(16));
}

std::set<Box*> global_sink;

void moved_out_ok() {
  std::set<Box*> s;
  s.insert((Box*)malloc(16));
  global_sink = std::move(s);
}

void swapped_out_ok() {
  std::set<Box*> s;
  s.insert((Box*)malloc(16));
  global_sink.swap(s);
}

std::set<Box*> returned_ok() {
  std::set<Box*> s;
  s.insert((Box*)malloc(16));
  return s;
}

void heap_container_ok(std::set<Box*>* s) { s->insert((Box*)malloc(16)); }

void cleared_then_reused_ok(std::set<Box*>& s) {
  s.clear();
  s.insert((Box*)malloc(16));
}

// Controls on paths that already worked.
void vector_push_back_ok(std::vector<Box*>& v) {
  v.push_back((Box*)malloc(16));
}

// TRUE POSITIVES: the container is local, so what it owns dies with it. These keep
// reporting only because the container has a destructor model.
void local_set_insert_bad() {
  std::set<Box*> s;
  s.insert((Box*)malloc(16));
}

void local_unordered_set_insert_bad() {
  std::unordered_set<Box*> s;
  s.insert((Box*)malloc(16));
}

struct Owner {
  std::set<Box*> s;
};

void local_member_set_bad() {
  Owner o;
  o.s.insert((Box*)malloc(16));
}

// An entry of a map-like container is a pair, and either half of it can be the
// owner; container_owning_key.cpp covers the key side.
void map_insert_ok(std::map<int, Box*>& m) { m.insert({1, (Box*)malloc(16)}); }

void unordered_map_insert_ok(std::unordered_map<int, Box*>& m) {
  m.insert({1, (Box*)malloc(16)});
}

void multimap_insert_ok(std::multimap<int, Box*>& m) {
  m.insert({1, (Box*)malloc(16)});
}

void unordered_multimap_insert_ok(std::unordered_multimap<int, Box*>& m) {
  m.insert({1, (Box*)malloc(16)});
}

void map_index_ok(std::map<int, Box*>& m) { m[1] = (Box*)malloc(16); }

// TRUE POSITIVE: tying the mapped value to the container does not save it when
// the container itself is local.
void local_map_insert_bad() {
  std::map<int, Box*> m;
  m.insert({1, (Box*)malloc(16)});
}

void plain_leak_bad() { malloc(16); }

} // namespace container_insert_ownership
