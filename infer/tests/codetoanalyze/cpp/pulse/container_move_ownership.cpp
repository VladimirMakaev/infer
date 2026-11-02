/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <list>
#include <map>
#include <set>
#include <utility>

namespace container_move_ownership {

// Returning a container by value moves it into the caller's object and then
// destroys the source. The elements belong to the result, so nothing the caller
// reads through it is invalid.
std::map<int, int> make_map() {
  std::map<int, int> m;
  m[1] = 1;
  return m;
}

int return_by_value_ok() {
  std::map<int, int> m = make_map();
  return m.at(1);
}

int return_by_value_iterator_ok() {
  std::map<int, int> m = make_map();
  return m.begin()->second;
}

// Two returns: the result is move-constructed from a named local rather than
// built in place.
std::set<int> make_set(bool b) {
  std::set<int> a, c;
  a.insert(1);
  c.insert(2);
  if (b) {
    return a;
  }
  return c;
}

int return_by_value_two_returns_ok(bool b) {
  std::set<int> s = make_set(b);
  return *s.begin();
}

// Moving into a member: the by-value parameter dies at the end of the
// constructor, after handing its elements to the member.
struct Holder {
  std::map<int, int> m;
  explicit Holder(std::map<int, int> other) : m(std::move(other)) {}
};

int move_into_member_ok() {
  std::map<int, int> m;
  m[1] = 1;
  Holder h(std::move(m));
  return h.m.at(1);
}

// Moving into a local that outlives the source.
int move_into_local_ok() {
  std::list<int> dst;
  {
    std::list<int> src;
    src.push_back(1);
    std::list<int> moved(std::move(src));
    dst = std::move(moved);
  }
  return dst.front();
}

// An element reached before the move is still the destination's element.
int move_keeps_element_alive_ok() {
  std::set<int> dst;
  const int* p = nullptr;
  {
    std::set<int> src;
    src.insert(1);
    p = &*src.begin();
    dst = std::set<int>(std::move(src));
  }
  return *p;
}

// Swapping does not destroy anything, so both containers keep their elements.
int swap_ok() {
  std::set<int> a, b;
  a.insert(1);
  a.swap(b);
  return b.empty() ? 0 : 1;
}

// Controls: a container that really is destroyed still invalidates what it
// held.
int local_dies_bad() {
  const int* p = nullptr;
  {
    std::set<int> s;
    s.insert(1);
    p = &*s.begin();
  }
  return *p;
}

int destination_dies_bad() {
  const int* p = nullptr;
  {
    std::set<int> src;
    src.insert(1);
    std::set<int> dst(std::move(src));
    p = &*dst.begin();
  }
  return *p;
}

} // namespace container_move_ownership
