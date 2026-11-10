/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <new>

// Popping twice from a list. Unlinking a node writes the node's successor through the node's own
// back link, which is the sentinel when the node is the first one; with the implementation
// analysed those two could not be related, so the sentinel still pointed at the node the first
// pop destroyed and the second pop read it.
void list_two_pops_ok(std::list<int>& l) {
  if (l.size() >= 2) {
    l.pop_front();
    l.pop_front();
  }
}

void list_pop_front_loop_ok(std::list<int>& l) {
  while (!l.empty()) {
    l.pop_front();
  }
}

void list_pop_back_loop_ok(std::list<int>& l) {
  while (!l.empty()) {
    l.pop_back();
  }
}

// a bounded trim followed by an insertion, the shape a fixed-size history buffer uses
void list_trim_then_push_ok(std::list<int>& l, int v) {
  while (l.size() >= 8) {
    l.pop_front();
  }
  l.push_back(v);
}

// draining in a destructor, with an element type that owns memory
struct Owner {
  std::list<std::unique_ptr<int>> items_;
  ~Owner() {
    while (!items_.empty()) {
      items_.pop_back();
    }
  }
};

void owner_drains_in_destructor_ok() { Owner o; }

void deque_two_pops_ok(std::deque<int>& d) {
  if (d.size() >= 2) {
    d.pop_front();
    d.pop_back();
  }
}

// Popping an empty container is undefined; the model assumes the container is non-empty, so a
// guarded pop stays silent rather than becoming a report on every caller.
void pop_guarded_by_empty_ok(std::list<int>& l) {
  if (!l.empty()) {
    l.pop_front();
  }
}

// The pop takes the value away, so copying it out first is what keeps it alive: the copy is
// necessary and must not be reported as unnecessary.
int copy_out_then_pop_ok(std::list<int>& l) {
  if (l.empty()) {
    return 0;
  }
  auto v = l.front();
  l.pop_front();
  return v;
}

// A reference to an end of the container, and then the pop that removes that same end: the
// reference is left dangling.
int list_front_then_pop_front_bad(std::list<int>& l) {
  if (l.empty()) {
    return 0;
  }
  int& r = l.front();
  l.pop_front();
  return r;
}

int list_back_then_pop_back_bad(std::list<int>& l) {
  if (l.empty()) {
    return 0;
  }
  int& r = l.back();
  l.pop_back();
  return r;
}

int deque_front_then_pop_front_bad(std::deque<int>& d) {
  if (d.empty()) {
    return 0;
  }
  int& r = d.front();
  d.pop_front();
  return r;
}

// Popping the OTHER end leaves the reference alone, which is why the ends are modelled as the
// cells at 0 and at size - 1 rather than the whole array being invalidated.
int list_front_then_pop_back_ok(std::list<int>& l) {
  if (l.size() < 2) {
    return 0;
  }
  int& r = l.front();
  l.pop_back();
  return r;
}

int list_back_then_pop_front_ok(std::list<int>& l) {
  if (l.size() < 2) {
    return 0;
  }
  int& r = l.back();
  l.pop_front();
  return r;
}

// With one element the two ends are the same object, so popping either one does dangle the
// reference taken from the other.
int list_single_element_front_then_pop_back_bad(std::list<int>& l) {
  if (l.size() != 1) {
    return 0;
  }
  int& r = l.front();
  l.pop_back();
  return r;
}

// The ends reached through an iterator. `begin()` names the same cell as `front()`, so a
// reference taken through it and the pop that removes the front are one address; an iterator with
// no position, such as the one `find()` returns, still names a cell of its own.
int list_begin_then_pop_front_bad(std::list<int>& l) {
  if (l.empty()) {
    return 0;
  }
  int& r = *l.begin();
  l.pop_front();
  return r;
}

int list_begin_read_after_pop_front_bad(std::list<int>& l) {
  if (l.empty()) {
    return 0;
  }
  std::list<int>::iterator it = l.begin();
  l.pop_front();
  return *it;
}

int deque_begin_then_pop_front_bad(std::deque<int>& d) {
  if (d.empty()) {
    return 0;
  }
  int& r = *d.begin();
  d.pop_front();
  return r;
}

// popping the other end leaves it alone, exactly as it does for `front()`
int list_begin_then_pop_back_ok(std::list<int>& l) {
  if (l.size() < 2) {
    return 0;
  }
  int& r = *l.begin();
  l.pop_back();
  return r;
}

// Erasing the front removes one element and the rest take new positions, so the front afterwards
// is a different object. This is why `erase` forgets the backing array as a pop does: without
// that, the cell `begin()` named would still be the invalidated one and these would report.
int list_erase_begin_then_front_ok(std::list<int>& l) {
  if (l.size() < 2) {
    return 0;
  }
  l.erase(l.begin());
  return l.front();
}

int list_erase_begin_then_begin_ok(std::list<int>& l) {
  if (l.size() < 2) {
    return 0;
  }
  l.erase(l.begin());
  return *l.begin();
}

int deque_erase_begin_then_front_ok(std::deque<int>& d) {
  if (d.size() < 2) {
    return 0;
  }
  d.erase(d.begin());
  return d.front();
}

// Erasing an element that is NOT an end leaves the ends where they were, so a reference taken
// from the front still meets the pop that removes it. This is why the array is forgotten only
// when the erased cell is the one the ends name: forgetting it on every erase would drop this.
int list_front_erase_other_then_pop_front_bad(std::list<int>& l) {
  if (l.size() < 2) {
    return 0;
  }
  int& r = l.front();
  std::list<int>::iterator it = l.begin();
  ++it;
  l.erase(it);
  l.pop_front();
  return r;
}

int deque_front_erase_other_then_pop_front_bad(std::deque<int>& d) {
  if (d.size() < 2) {
    return 0;
  }
  int& r = d.front();
  std::deque<int>::iterator it = d.begin();
  ++it;
  d.erase(it);
  d.pop_front();
  return r;
}

// erasing the front through the iterator that names it does dangle the reference
int list_begin_then_erase_begin_bad(std::list<int>& l) {
  if (l.empty()) {
    return 0;
  }
  int& r = *l.begin();
  l.erase(l.begin());
  return r;
}

// ordinary traversal, and erasing while traversing, must stay silent
int list_iterate_ok(std::list<int>& l) {
  int n = 0;
  for (std::list<int>::iterator it = l.begin(); it != l.end(); ++it) {
    n += *it;
  }
  return n;
}

int list_erase_in_loop_ok(std::list<int>& l) {
  for (std::list<int>::iterator it = l.begin(); it != l.end();) {
    if (*it == 0) {
      it = l.erase(it);
    } else {
      ++it;
    }
  }
  return 0;
}

// An associative container has no ends to meet, so its `begin()` keeps naming a cell of its own:
// giving it the first one would make these two iterators the same object and the erase would
// dangle the second.
int map_erase_begin_then_begin_ok(std::map<int, int>& m) {
  if (m.size() < 2) {
    return 0;
  }
  m.erase(m.begin());
  return m.begin()->second;
}

// A node reached through a raw pointer, which no container model covers: ending its lifetime with
// an explicit destructor call and then reading it is a real use-after-lifetime and must keep
// reporting.
struct Node {
  Node* next;
  int v;
};

int explicit_destroy_then_read_bad(Node* n) {
  n->~Node();
  return n->v;
}

struct Chain {
  Node* head;

  int pop_and_read_bad() {
    Node* n = head;
    head = n->next;
    n->~Node();
    return n->v;
  }
};

// a new lifetime in the same storage makes the read fine again
int explicit_destroy_reconstruct_read_ok(Node* n) {
  n->~Node();
  ::new (n) Node{nullptr, 0};
  return n->v;
}

// Known limitation, and the reason the fix is a container model rather than a change to the
// lifetime rules: a hand-written intrusive list has the same sentinel shape, and unlinking still
// writes through a back link that cannot be related to the sentinel, so the second pop reads the
// node the first one destroyed. This is an aliasing limitation. Written without the emptiness
// guard, the same shape also reports a use-after-delete when the node is freed with `delete`,
// which long predates the explicit-destructor lifetime rule.
struct Link {
  Link* next;
  Link* prev;
};

struct Sentinel {
  Link end_;

  void unlink(Link* n) {
    n->prev->next = n->next;
    n->next->prev = n->prev;
  }

  void pop_destroy() {
    Link* n = end_.next;
    if (n == nullptr || n == &end_) {
      return;
    }
    unlink(n);
    n->~Link();
  }
};

void FP_intrusive_two_pops_ok(Sentinel& s) {
  s.pop_destroy();
  s.pop_destroy();
}

void intrusive_one_pop_ok(Sentinel& s) { s.pop_destroy(); }
