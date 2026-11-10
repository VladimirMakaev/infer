/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// An explicit `p->~T()` ends the object's lifetime but leaves the storage
// alone. Every `_ok` here is a shape that a naive "invalidate the receiver on
// any destructor call" would break, so they are the controls, not decoration.
#include <new>
#include <string>
#include <vector>

namespace explicit_destructor {

struct Plain {
  int n;
  Plain() : n(1) {}
  ~Plain() {}
};

struct Owner {
  int* p;
  Owner() : p(new int(1)) {}
  ~Owner() { delete p; }
};

int destruct_read_plain_field_bad() {
  alignas(Plain) char buf[sizeof(Plain)];
  Plain* p = ::new (buf) Plain();
  p->~Plain();
  return p->n;
}

int destruct_write_plain_field_bad() {
  alignas(Plain) char buf[sizeof(Plain)];
  Plain* p = ::new (buf) Plain();
  p->~Plain();
  p->n = 7;
  return p->n;
}

// the manual-lifetime idiom: a union member destroyed and reconstructed in
// place by small member functions, so the destructor call is in a callee.
template <class T>
struct Slot {
  union {
    T value_;
  };
  Slot() {}
  ~Slot() {}
  void destruct() { value_.~T(); }
  void construct() { ::new (&value_) T(); }
  T& get() { return value_; }
};

int union_destruct_then_read_bad() {
  Slot<Plain> s;
  s.construct();
  s.destruct();
  return s.get().n;
}

// A trivially destructible type has nothing to run, but the call still ends the
// object's lifetime, so this reports. That is what the standard says and it is
// the widest shape this check covers: a reader who expects only destructors
// with a body to matter will meet it here first.
struct Pod {
  int n;
};
int pod_destruct_then_read_bad() {
  Pod s;
  s.n = 3;
  s.~Pod();
  return s.n;
}

// ---- already reported before this change; they must keep reporting ----

int delete_then_read_bad() {
  Owner* o = new Owner();
  delete o;
  return *o->p;
}

int double_explicit_destruct_bad() {
  Owner o;
  o.~Owner();
  o.~Owner();
  return 0;
}

// ---- controls that must stay silent ----

// the point of destroying in place: reconstruct, then use.
int destruct_reconstruct_read_ok() {
  alignas(Plain) char buf[sizeof(Plain)];
  Plain* p = ::new (buf) Plain();
  p->~Plain();
  p = ::new (buf) Plain();
  return p->n;
}

// the same through the union slot, so both the destroy and the reconstruct sit
// in a callee and the caller sees only their summaries.
int union_destruct_reconstruct_read_ok() {
  Slot<Plain> s;
  s.construct();
  s.destruct();
  s.construct();
  return s.get().n;
}

// the destructor is the last thing that happens.
int destruct_last_ok() {
  alignas(Plain) char buf[sizeof(Plain)];
  Plain* p = ::new (buf) Plain();
  int v = p->n;
  p->~Plain();
  return v;
}

// an ordinary object going out of scope. The injected destructor destroys the
// members; none of that may invalidate the object while it is still running.
struct HasMembers {
  Plain a;
  Owner b;
  std::string c;
  ~HasMembers() {}
};
void normal_scope_exit_ok() { HasMembers h; }

// the destructor runs on a different object of the same type.
int other_object_untouched_ok() {
  alignas(Plain) char b1[sizeof(Plain)];
  alignas(Plain) char b2[sizeof(Plain)];
  Plain* x = ::new (b1) Plain();
  Plain* y = ::new (b2) Plain();
  x->~Plain();
  return y->n;
}

// "delete this": destroy in place, then hand the storage to operator delete.
// Passing the pointer as an argument is not a dereference.
struct Suicide {
  int n;
  void kill() {
    this->~Suicide();
    ::operator delete(this);
  }
};
void delete_this_ok(Suicide* s) { s->kill(); }

// after the destructor the pointer is only compared and reassigned.
int compare_and_reassign_ok() {
  alignas(Plain) char buf[sizeof(Plain)];
  Plain* p = ::new (buf) Plain();
  p->~Plain();
  if (p == nullptr) {
    return 0;
  }
  p = nullptr;
  return p == nullptr ? 1 : 2;
}

// the allocator destroys elements inside the container; ordinary use of a
// container must not start reporting.
int vector_ok() {
  std::vector<Plain> v;
  v.push_back(Plain());
  v.push_back(Plain());
  v.clear();
  v.push_back(Plain());
  return (int)v.size();
}

// destroy and reconstruct in a loop.
int destruct_reconstruct_loop_ok(int k) {
  alignas(Plain) char buf[sizeof(Plain)];
  Plain* p = ::new (buf) Plain();
  for (int i = 0; i < k; i++) {
    p->~Plain();
    p = ::new (buf) Plain();
  }
  int v = p->n;
  p->~Plain();
  return v;
}

// only a subobject was destroyed; the sibling is still alive.
struct Pair {
  Plain first;
  Plain second;
  ~Pair() {}
};
int subobject_destroyed_other_read_ok() {
  alignas(Pair) char buf[sizeof(Pair)];
  Pair* p = ::new (buf) Pair();
  p->first.~Plain();
  return p->second.n;
}

// `dealloc` is the name of the Objective-C deallocator, and it is also a legal
// name for a C++ method. A call to one is an ordinary method call, not
// `p->~T()`, so it must not end the lifetime of its receiver. The two shapes
// below are the ones real libraries use: a container handed a block whose
// elements it destroys, and a buffer that destroys what it owns.
struct Block {
  Plain slots[4];
};

struct Container {
  int size;
  Block* block;

  void dealloc(Block* b) { b->slots[0].~Plain(); }

  // the container itself was never destroyed, only an element of the block
  int release_then_read_own_field_ok() {
    dealloc(block);
    return size;
  }

  // the element really is gone, so reading it is still an error
  int release_then_read_element_bad() {
    dealloc(block);
    return block->slots[0].n;
  }
};

struct Buffer {
  Plain* data;
  int capacity;

  void dealloc() { data->~Plain(); }

  int clear_then_read_own_field_ok() {
    dealloc();
    return capacity;
  }

  int clear_then_read_data_bad() {
    dealloc();
    return data->n;
  }
};

// a `dealloc` that really does end its own receiver's lifetime, which is the
// one thing the change must not stop reporting
struct SelfFreeing {
  int v;
  ~SelfFreeing();
  void dealloc() { this->~SelfFreeing(); }
};

int self_dealloc_then_read_bad(SelfFreeing* s) {
  s->dealloc();
  return s->v;
}

// a method whose name is not `dealloc` behaved correctly all along, and is the
// control that says the two cases above are about the name and nothing else
struct Releaser {
  int size;
  Block* block;

  void release(Block* b) { b->slots[0].~Plain(); }

  int release_then_read_own_field_ok() {
    release(block);
    return size;
  }

  int release_then_read_element_bad() {
    release(block);
    return block->slots[0].n;
  }
};

} // namespace explicit_destructor
