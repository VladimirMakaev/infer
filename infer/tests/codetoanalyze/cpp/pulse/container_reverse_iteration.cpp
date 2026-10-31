/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstdlib>
#include <deque>
#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace container_reverse_iteration {

struct Box {
  char* p;
};

// rbegin and rend return iterators into the container, so an owner stored
// through one is held by the container and is not leaked.
//
// KNOWN LIMITATION, std::vector and std::string only, and only with libc++:
// binding the base iterator is enough for the node-based containers because
// their iterator dereference is modelled, but the array-backed iterator model
// records the position in a Pulse-internal field, and libc++'s __wrap_iter
// dereference is real code that reads its own member instead. The same two
// cases are silent when the tests are built against libstdc++, whose
// __normal_iterator dereference the models do cover. These are the FPs that
// were here before this change; it does not add any.
void FP_vector_reverse_loop_ok(std::vector<Box>& v) {
  for (auto it = v.rbegin(); it != v.rend(); ++it) {
    it->p = (char*)malloc(16);
  }
}

void FP_vector_rbegin_ok(std::vector<Box>& v) {
  auto it = v.rbegin();
  if (it != v.rend()) {
    it->p = (char*)malloc(16);
  }
}

void map_rbegin_ok(std::map<int, Box>& m) {
  auto it = m.rbegin();
  if (it != m.rend()) {
    it->second.p = (char*)malloc(16);
  }
}

void map_reverse_loop_ok(std::map<int, Box>& m) {
  for (auto it = m.rbegin(); it != m.rend(); ++it) {
    it->second.p = (char*)malloc(16);
  }
}

void map_rend_step_back_ok(std::map<int, Box>& m) {
  auto it = m.rend();
  if (it != m.rbegin()) {
    --it;
    it->second.p = (char*)malloc(16);
  }
}

void multimap_rbegin_ok(std::multimap<int, Box>& m) {
  auto it = m.rbegin();
  if (it != m.rend()) {
    it->second.p = (char*)malloc(16);
  }
}

void deque_rbegin_ok(std::deque<Box>& d) {
  auto it = d.rbegin();
  if (it != d.rend()) {
    it->p = (char*)malloc(16);
  }
}

void list_rbegin_ok(std::list<Box>& l) {
  auto it = l.rbegin();
  if (it != l.rend()) {
    it->p = (char*)malloc(16);
  }
}

void vector_crbegin_ok(std::vector<Box>& v, std::map<int, Box>& m) {
  auto it = v.crbegin();
  if (it != v.crend()) {
    m[0].p = (char*)malloc(16);
  }
}

// A set's elements are const, so nothing can be stored through a reverse
// iterator to one; reading must stay silent.
void set_rbegin_read_ok(std::set<int>& s) {
  auto it = s.rbegin();
  if (it != s.rend()) {
    int v = *it;
    (void)v;
  }
}

// A string's elements are chars, so no owner can be stored through one either.
void string_rbegin_read_ok(std::string& s) {
  auto it = s.rbegin();
  if (it != s.rend()) {
    char c = *it;
    (void)c;
  }
}

// TRUE POSITIVE: a local container dying while holding an owner stored through
// a reverse iterator is reported, exactly as it is for a forward iterator. The
// container has to be non-empty for the store to happen at all.
void local_vector_reverse_dies_bad() {
  std::vector<Box> v;
  v.push_back(Box{nullptr});
  auto it = v.rbegin();
  if (it != v.rend()) {
    it->p = (char*)malloc(16);
  }
}

void local_map_reverse_dies_bad() {
  std::map<int, Box> m;
  m[1] = Box{nullptr};
  auto it = m.rbegin();
  if (it != m.rend()) {
    it->second.p = (char*)malloc(16);
  }
}

// The same shape on an EMPTY container: rbegin() == rend(), so the guarded
// store is unreachable in Pulse and in real execution alike. The silence is
// correct and is pinned deliberately.
void local_empty_map_reverse_guarded_store_ok() {
  std::map<int, Box> m;
  auto it = m.rbegin();
  if (it != m.rend()) {
    it->second.p = (char*)malloc(16);
  }
}

// Control: the forward iterators already behaved this way and must not change.
void vector_forward_loop_ok(std::vector<Box>& v) {
  for (auto it = v.begin(); it != v.end(); ++it) {
    it->p = (char*)malloc(16);
  }
}

// TRUE POSITIVE: nothing receives the allocation.
void plain_leak_bad() { malloc(16); }

// TRUE POSITIVE: the allocation is overwritten before it is ever stored into
// the container, so modelling reverse iteration must not hide it.
void reverse_overwritten_bad(std::vector<Box>& v) {
  char* p = (char*)malloc(16);
  p = (char*)malloc(16);
  auto it = v.rbegin();
  if (it != v.rend()) {
    it->p = p;
  }
}

} // namespace container_reverse_iteration
