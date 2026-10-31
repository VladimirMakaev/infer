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

namespace container_equal_range {

struct Box {
  char* p;
};

// equal_range returns a pair of iterators into the container. Both halves are
// iterators to its elements, so an owner stored through either is held by the
// container and is not leaked.
void equal_range_first_ok(std::map<int, Box>& m) {
  auto range = m.equal_range(1);
  if (range.first != m.end()) {
    range.first->second.p = (char*)malloc(16);
  }
}

void equal_range_second_ok(std::map<int, Box>& m) {
  auto range = m.equal_range(1);
  if (range.second != m.end()) {
    range.second->second.p = (char*)malloc(16);
  }
}

// the idiomatic spelling: both halves are bound, so both are usable
void equal_range_structured_binding_ok(std::map<int, Box>& m) {
  auto [lo, hi] = m.equal_range(1);
  if (lo != m.end()) {
    lo->second.p = (char*)malloc(16);
  }
  if (hi != m.end()) {
    hi->second.p = (char*)malloc(16);
  }
}

void equal_range_loop_ok(std::multimap<int, Box>& m) {
  auto range = m.equal_range(1);
  for (auto it = range.first; it != range.second; ++it) {
    it->second.p = (char*)malloc(16);
  }
}

void unordered_map_equal_range_ok(std::unordered_map<int, Box>& m) {
  auto range = m.equal_range(1);
  if (range.first != m.end()) {
    range.first->second.p = (char*)malloc(16);
  }
}

void multimap_equal_range_ok(std::multimap<int, Box>& m) {
  auto range = m.equal_range(1);
  if (range.first != m.end()) {
    range.first->second.p = (char*)malloc(16);
  }
}

void unordered_multimap_equal_range_ok(std::unordered_multimap<int, Box>& m) {
  auto range = m.equal_range(1);
  if (range.first != m.end()) {
    range.first->second.p = (char*)malloc(16);
  }
}

// Control: find and lower_bound already behaved this way and must not change.
void find_store_ok(std::map<int, Box>& m) {
  auto it = m.find(1);
  if (it != m.end()) {
    it->second.p = (char*)malloc(16);
  }
}

// The elements of a set are const, so there is nothing to store through an
// iterator equal_range returns on one; it is modelled for the map-like
// containers only. Reading through the iterator must stay silent.
void set_equal_range_read_ok(std::set<int>& s) {
  auto range = s.equal_range(1);
  if (range.first != s.end()) {
    int v = *range.first;
    (void)v;
  }
}

// TRUE POSITIVE: nothing receives the allocation.
void plain_leak_bad() { malloc(16); }

// TRUE POSITIVE: the first allocation is overwritten before it is ever stored
// into the container, so modelling equal_range must not hide it.
void equal_range_overwritten_bad(std::map<int, Box>& m) {
  char* p = (char*)malloc(16);
  p = (char*)malloc(16);
  auto range = m.equal_range(1);
  if (range.first != m.end()) {
    range.first->second.p = p;
  }
}

// TRUE POSITIVE: `clear` invalidates the iterators obtained before it. Once
// equal_range returns real iterators these are checked, exactly as they already
// are for `find`, `lower_bound` and `begin`.
void equal_range_after_clear_bad(std::map<int, Box>& m) {
  auto range = m.equal_range(1);
  m.clear();
  range.first->second.p = nullptr;
}

// TRUE POSITIVE: equal_range on an empty container returns a pair of end
// iterators, and dereferencing one is undefined. Same as `find` on an empty
// container, which already reports.
void equal_range_empty_map_deref_bad() {
  std::map<int, Box> m;
  auto range = m.equal_range(1);
  range.first->second.p = nullptr;
}

// TRUE POSITIVE: a local container dying while holding an owner stored through
// an iterator is reported. The iterator's element is a cell of the backing
// array, so the container's death makes the owner unreachable. The map has to
// be non-empty for the store to happen at all -- see the companion below.
void local_map_equal_range_dies_bad() {
  std::map<int, Box> m;
  m[1] = Box{nullptr};
  auto range = m.equal_range(1);
  if (range.first != m.end()) {
    range.first->second.p = (char*)malloc(16);
  }
}

// The same shape on an EMPTY map. equal_range then returns a pair of end
// iterators, so the guarded store is unreachable -- in Pulse and in real
// execution alike. Nothing is allocated on any path, so the silence here is
// correct and is pinned deliberately rather than mistaken for a limitation.
void local_empty_map_guarded_store_ok() {
  std::map<int, Box> m;
  auto range = m.equal_range(1);
  if (range.first != m.end()) {
    range.first->second.p = (char*)malloc(16);
  }
}

} // namespace container_equal_range
