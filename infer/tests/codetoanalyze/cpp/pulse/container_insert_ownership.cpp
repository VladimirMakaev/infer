/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstdlib>
#include <map>
#include <set>
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

// KNOWN LIMITATION: returning the container by value is still reported. std::vector
// does not have this problem, so it is a remaining gap in node-container parity, not
// something this change introduced -- it was reported before the change too.
std::set<Box*> FP_returned_ok() {
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

// KNOWN LIMITATION: a map's element is a pair, not a scalar, so the inserted value
// is not tied and this is still reported. Recorded rather than hidden.
void FP_map_insert_ok(std::map<int, Box*>& m) {
  m.insert({1, (Box*)malloc(16)});
}

void plain_leak_bad() { malloc(16); }

} // namespace container_insert_ownership
