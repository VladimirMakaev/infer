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

// KNOWN LIMITATION, shared with the already-modelled `find` and `begin`: once an
// iterator is tied to the container, a local container dying does not make what it
// holds unreachable, so this leak is missed. Modelling lower_bound does not create
// the gap, it joins it; on an unmodelled operation the leak was caught by accident.
void FN_local_map_lower_bound_bad() {
  std::map<int, Box> m;
  auto it = m.lower_bound(1);
  if (it != m.end()) {
    it->second.p = (char*)malloc(16);
  }
}

} // namespace container_bound_lookup
