/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstdlib>
#include <set>
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

// TRUE POSITIVES: the container is local, so what it owns dies with it.
void local_emplace_bad() {
  std::set<Box*> s;
  s.emplace((Box*)malloc(16));
}

void local_unordered_emplace_bad() {
  std::unordered_set<Box*> s;
  s.emplace((Box*)malloc(16));
}

// The size is tracked, so the container is no longer empty after an emplace.
bool emplace_then_empty_ok() {
  std::set<int> s;
  s.emplace(1);
  return s.empty();
}

void plain_leak_bad() { malloc(16); }

} // namespace container_emplace
