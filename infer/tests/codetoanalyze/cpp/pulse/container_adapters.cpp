/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
#include <cstdlib>
#include <deque>
#include <map>
#include <queue>
#include <stack>
#include <vector>

namespace container_adapters {

struct Box {
  char* p;
};

// An owner stored through an element accessor of a container adapter: the memory stays
// reachable from the adapter, which outlives the call.
void stack_top_ok(std::stack<Box*>& s) { s.top()->p = (char*)malloc(16); }

void queue_front_ok(std::queue<Box*>& q) { q.front()->p = (char*)malloc(16); }

void queue_back_ok(std::queue<Box*>& q) { q.back()->p = (char*)malloc(16); }

void priority_queue_top_ok(std::priority_queue<Box*>& q) {
  q.top()->p = (char*)malloc(16);
}

// The const overload of the accessor reaches the same element.
void stack_top_const_ok(const std::stack<Box*>& s) {
  s.top()->p = (char*)malloc(16);
}

// The adapter is a member of a live object rather than a parameter.
struct Holder {
  std::stack<Box*> s;
  std::queue<Box*> q;

  void member_stack_top_ok() { s.top()->p = (char*)malloc(16); }
  void member_queue_front_ok() { q.front()->p = (char*)malloc(16); }
};

// The element written by an insertion is read back: the accessor must not contradict it.
void push_then_top_ok(std::stack<Box*>& s, Box* b) {
  s.push(b);
  s.top()->p = (char*)malloc(16);
}

// An owner passed INTO an insertion, which is not modelled for the adapters: the argument is
// havocked, so the allocation is not tracked past the call. Modelling an insertion without
// making the inserted value reachable from the adapter would turn these into false positives,
// which is why this commit models the element accessors only.
void stack_push_ok(std::stack<char*>& s) { s.push((char*)malloc(16)); }

void queue_push_ok(std::queue<char*>& q) { q.push((char*)malloc(16)); }

void priority_queue_push_ok(std::priority_queue<char*>& q) {
  q.push((char*)malloc(16));
}

// The same insertion into an adapter that dies at the end of the function really does leak.
// Havocking the argument of the unmodelled insertion drops the allocation, so it is missed;
// that is the behaviour before this commit as well.
void FN_stack_push_local_dies_bad() {
  std::stack<char*> s;
  s.push((char*)malloc(16));
}

// The already modelled containers, unchanged by this commit.
void deque_back_ok(std::deque<Box*>& d) { d.back()->p = (char*)malloc(16); }

void vector_back_ok(std::vector<Box*>& v) { v.back()->p = (char*)malloc(16); }

void map_at_ok(std::map<int, Box>& m) { m.at(0).p = (char*)malloc(16); }

// Nothing holds on to the memory.
void plain_leak_bad() { char* p = (char*)malloc(16); }

// The only pointer to the memory is overwritten.
void overwritten_bad() {
  Box b;
  b.p = (char*)malloc(16);
  b.p = nullptr;
}

// operator new, to cover MEMORY_LEAK_CPP as well.
void new_leak_bad() { Box* b = new Box(); }

// The adapter and the element it holds are both local, so the memory really does leak.
// Reporting it needs the element of a dead container to be seen as dead, which the element
// model of the sequence containers does not do: std::deque, std::list and std::map all miss
// the same shape already, and only std::vector catches it.
void FN_stack_local_dies_bad() {
  std::stack<Box*> s;
  Box b;
  b.p = nullptr;
  s.push(&b);
  s.top()->p = (char*)malloc(16);
}

// The same shape on std::deque. This was a known false negative when the node-based
// containers had no destructor model; it reports now that they do.
void deque_local_dies_bad() {
  std::deque<Box*> d;
  Box b;
  b.p = nullptr;
  d.push_back(&b);
  d.back()->p = (char*)malloc(16);
}

} // namespace container_adapters
