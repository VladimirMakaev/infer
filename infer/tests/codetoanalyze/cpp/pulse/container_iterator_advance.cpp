/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <deque>
#include <list>
#include <map>
#include <vector>

// Destroying every element of a container in place and stepping the iterator on
// is what a container's own clear() does. Stepping reads the node links, or the
// deque's position, and not the element, so an occupant destroyed in place
// leaves the iterator usable. The storage still has to be there, which is what
// keeps erase, clear and free reportable at the step.

namespace container_iterator_advance {

struct Heavy {
  int* p;
  Heavy() : p(nullptr) {}
  ~Heavy() {}
};

void sink(int*);

// The shape libc++'s own deque::clear has: destroy each element, then advance.
void deque_destroy_loop_ok(std::deque<Heavy>& d) {
  for (std::deque<Heavy>::iterator i = d.begin(); i != d.end(); ++i) {
    i->~Heavy();
  }
}

void list_destroy_loop_ok(std::list<Heavy>& l) {
  for (std::list<Heavy>::iterator i = l.begin(); i != l.end(); ++i) {
    i->~Heavy();
  }
}

// std::vector's iterator is a different model and a raw range is no model at
// all: both were already silent, and both must stay that way.
void vector_destroy_loop_ok(std::vector<Heavy>& v) {
  for (std::vector<Heavy>::iterator i = v.begin(); i != v.end(); ++i) {
    i->~Heavy();
  }
}

void raw_range_destroy_loop_ok(Heavy* first, Heavy* last) {
  for (Heavy* i = first; i != last; ++i) {
    i->~Heavy();
  }
}

// No loop is needed to show it: one destroy and one step.
void destroy_then_advance_ok(std::deque<Heavy>& d) {
  std::deque<Heavy>::iterator i = d.begin();
  i->~Heavy();
  ++i;
}

// A pass that only reads, and one that only writes, never looked at a destroyed
// element: they are the controls that say the loop itself is not the cause.
void read_only_loop_ok(std::deque<Heavy>& d) {
  for (std::deque<Heavy>::iterator i = d.begin(); i != d.end(); ++i) {
    sink(i->p);
  }
}

void write_through_iterator_loop_ok(std::deque<Heavy>& d) {
  for (std::deque<Heavy>::iterator i = d.begin(); i != d.end(); ++i) {
    i->p = nullptr;
  }
}

// Destroying one element and reading a different one is not an error.
int destroy_front_read_back_ok(std::deque<Heavy>& d) {
  if (d.size() < 2) {
    return 0;
  }
  d.front().~Heavy();
  return d.back().p == nullptr ? 1 : 0;
}

// Two iterators from begin(), one of them stepped, name different elements.
void two_iterators_ok(std::deque<Heavy>& d) {
  std::deque<Heavy>::iterator a = d.begin();
  std::deque<Heavy>::iterator b = d.begin();
  ++b;
  a->~Heavy();
  sink(b->p);
}

// TRUE POSITIVES, each one a case the step could have been made to miss.

// erase and clear take the storage away, not just the occupant, so stepping an
// iterator over them is still a use of what is gone.
void list_erase_then_advance_bad(std::list<Heavy>& l) {
  std::list<Heavy>::iterator i = l.begin();
  l.erase(i);
  ++i;
}

void map_erase_then_advance_bad(std::map<int, Heavy>& m) {
  std::map<int, Heavy>::iterator i = m.begin();
  m.erase(i);
  ++i;
}

void deque_clear_then_advance_bad(std::deque<Heavy>& d) {
  std::deque<Heavy>::iterator i = d.begin();
  d.clear();
  ++i;
}

// Reading the element is a use of the object, not of its storage.
void destroy_then_read_bad(std::deque<Heavy>& d) {
  std::deque<Heavy>::iterator i = d.begin();
  i->~Heavy();
  sink(i->p);
}

void destroy_then_read_through_front_bad(std::deque<Heavy>& d) {
  std::deque<Heavy>::iterator i = d.begin();
  i->~Heavy();
  sink(d.front().p);
}

void destroy_front_twice_bad(std::deque<Heavy>& d) {
  d.front().~Heavy();
  d.front().~Heavy();
}

// begin() and front() name one element, so a procedure that steps an iterator
// and then reads the front puts both requirements on the same address. Only one
// is kept per address, so the two orders below have to agree: if the weaker one
// could hide the stronger, exactly one of these two callers would fall silent.
void advance_then_read_front(std::deque<Heavy>& d) {
  std::deque<Heavy>::iterator i = d.begin();
  ++i;
  sink(d.front().p);
}

void read_front_then_advance(std::deque<Heavy>& d) {
  std::deque<Heavy>::iterator i = d.begin();
  sink(d.front().p);
  ++i;
}

void advance_then_read_front_bad(std::deque<Heavy>& d) {
  d.front().~Heavy();
  advance_then_read_front(d);
}

void read_front_then_advance_bad(std::deque<Heavy>& d) {
  d.front().~Heavy();
  read_front_then_advance(d);
}

// ...and the same caller over a callee that does not step at all, which nothing
// here can excuse: if this one moves, something other than the step did it.
void read_front_only(std::deque<Heavy>& d) { sink(d.front().p); }

void read_front_only_bad(std::deque<Heavy>& d) {
  d.front().~Heavy();
  read_front_only(d);
}

// The caller-side shape of the silent cases above: destroying the front and
// calling a procedure that only steps is not an error anywhere.
void advance_only(std::deque<Heavy>& d) {
  std::deque<Heavy>::iterator i = d.begin();
  ++i;
}

void caller_destroy_then_advance_ok(std::deque<Heavy>& d) {
  d.front().~Heavy();
  advance_only(d);
}

} // namespace container_iterator_advance
