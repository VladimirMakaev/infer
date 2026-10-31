/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstdlib>
#include <map>
#include <set>

namespace container_bound_lookup {

struct Box {
  char* p;
};

// lower_bound and upper_bound return an iterator into the container, so an owner
// stored through one is held by the container and is not leaked.
void lower_bound_store_ok(std::map<int, Box>& m) {
  auto it = m.lower_bound(1);
  if (it != m.end()) {
    it->second.p = (char*)malloc(16);
  }
}

void upper_bound_store_ok(std::map<int, Box>& m) {
  auto it = m.upper_bound(1);
  if (it != m.end()) {
    it->second.p = (char*)malloc(16);
  }
}

void set_lower_bound_ok(std::set<int>& s, std::map<int, Box>& m) {
  auto it = s.lower_bound(1);
  if (it != s.end()) {
    m[*it].p = (char*)malloc(16);
  }
}

// Control: find already behaved this way and must not change.
void find_store_ok(std::map<int, Box>& m) {
  auto it = m.find(1);
  if (it != m.end()) {
    it->second.p = (char*)malloc(16);
  }
}

// TRUE POSITIVE: nothing receives the allocation.
void plain_leak_bad() { malloc(16); }

// TRUE POSITIVE: a local container dying while holding an owner stored through
// an iterator is reported. The iterator's element is a cell of the backing array,
// so the container's death makes the owner unreachable. The map has to be
// non-empty for the store to happen at all -- see the companion below.
void local_map_lower_bound_dies_bad() {
  std::map<int, Box> m;
  m[1] = Box{nullptr};
  auto it = m.lower_bound(1);
  if (it != m.end()) {
    it->second.p = (char*)malloc(16);
  }
}

// The same shape on an EMPTY map. lower_bound then returns end(), so the guarded
// store is unreachable -- in Pulse and in real execution alike. Nothing is
// allocated on any path, so the silence here is correct and is pinned
// deliberately rather than mistaken for a limitation.
void local_empty_map_guarded_store_ok() {
  std::map<int, Box> m;
  auto it = m.lower_bound(1);
  if (it != m.end()) {
    it->second.p = (char*)malloc(16);
  }
}

} // namespace container_bound_lookup
