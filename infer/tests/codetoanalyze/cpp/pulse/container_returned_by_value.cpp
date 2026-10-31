/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstdlib>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace container_returned_by_value {

struct Box {
  char* p;
};

// Returning a node container by value moves its elements to the caller, exactly
// as returning a std::vector does.
std::set<Box*> set_returned_ok() {
  std::set<Box*> s;
  s.insert((Box*)malloc(16));
  return s;
}

std::map<int, Box*> map_returned_ok() {
  std::map<int, Box*> m;
  m.insert({1, (Box*)malloc(16)});
  return m;
}

// The container can be a field of what is returned rather than the returned
// value.
struct Holder {
  std::set<Box*> s;
};

Holder holder_returned_ok() {
  Holder h;
  h.s.insert((Box*)malloc(16));
  return h;
}

// Control: the same shape over std::vector, which already worked.
std::vector<Box*> vector_returned_ok() {
  std::vector<Box*> v;
  v.push_back((Box*)malloc(16));
  return v;
}

std::set<Box*> move_constructed_ok() {
  std::set<Box*> s;
  s.insert((Box*)malloc(16));
  std::set<Box*> t(std::move(s));
  return t;
}

// KNOWN LIMITATION: only the move constructor hands the elements over, so a
// copy that outlives its source still looks like a leak. Modelling the copy
// constructor as std::vector does would fix this but would make the source of
// the copy look modified whenever an unmodelled method mutates the copy, losing
// const-refable advice.
std::set<Box*> FP_copy_constructed_escapes_ok() {
  std::set<Box*> s;
  s.insert((Box*)malloc(16));
  std::set<Box*> t(s);
  s.clear();
  return t;
}

// TRUE POSITIVES: a move that goes nowhere still loses what it holds.
void move_dies_bad() {
  std::set<Box*> s;
  s.insert((Box*)malloc(16));
  std::set<Box*> t(std::move(s));
}

void holder_dies_bad() {
  Holder h;
  h.s.insert((Box*)malloc(16));
}

void plain_leak_bad() { malloc(16); }

} // namespace container_returned_by_value
