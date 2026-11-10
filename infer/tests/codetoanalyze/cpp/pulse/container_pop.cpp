/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
#include <deque>
#include <list>
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
