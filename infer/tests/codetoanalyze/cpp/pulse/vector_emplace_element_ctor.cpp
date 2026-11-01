/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <memory>
#include <utility>
#include <vector>

namespace vector_emplace_element_ctor {

struct T {
  int x;
  T() : x(0) {}
  explicit T(int v) : x(v) {}
};

// An element of class type has to be CONSTRUCTED from the emplace arguments.
// The model used to store the argument as if it were already the element, and
// for a class element `read_pushed_value` ties nothing at all, so whatever the
// argument owned was left with no owner and read as a leak. Now the element's
// real constructor runs when it is captured, and when it is not -- libc++ never
// exposes unique_ptr's pointer-constructor to the frontend -- the argument is
// at least kept reachable from the vector, which is what an unmodelled
// container already gets.
void emplace_back_new_ok(std::vector<std::unique_ptr<T>>& v) {
  v.emplace_back(new T(1));
}

void emplace_back_raw_local_ok(std::vector<std::unique_ptr<T>>& v) {
  T* p = new T(2);
  v.emplace_back(p);
}

// a unique_ptr prvalue: the move constructor IS captured, so this one is
// really constructed. (Spelled without make_unique so the C++11 variant of
// this test directory compiles it too.)
void emplace_back_unique_ptr_temporary_ok(std::vector<std::unique_ptr<T>>& v) {
  v.emplace_back(std::unique_ptr<T>(new T(3)));
}

// A user-defined owner, whose constructor IS captured: this one is really
// constructed rather than merely tied.
struct Owner {
  T* p;
  explicit Owner(T* q) : p(q) {}
  ~Owner() { delete p; }
};

void emplace_back_owner_ok(std::vector<Owner>& v) { v.emplace_back(new T(4)); }

// A vector of raw pointers owns nothing, but the vector outlives the call, so
// the pointer stays reachable through it. Same convention as
// vector_insert_ownership.cpp's emplace_back_ok.
void emplace_back_raw_vector_ok(std::vector<T*>& v) { v.emplace_back(new T(5)); }

// Controls on the spellings that already worked, kept so a regression in them
// shows up here too.
void push_back_unique_ptr_ok(std::vector<std::unique_ptr<T>>& v) {
  v.push_back(std::unique_ptr<T>(new T(6)));
}

void emplace_back_moved_named_ok(std::vector<std::unique_ptr<T>>& v) {
  std::unique_ptr<T> u(new T(7));
  v.emplace_back(std::move(u));
}

// TRUE POSITIVES. The vector is local and owns nothing, so what was emplaced
// dies with it.
void local_emplace_back_raw_vector_bad() {
  std::vector<T*> v;
  v.emplace_back(new T(8));
}

void plain_new_leak_bad() { new T(9); }

// KNOWN LIMITATION, a different root cause and not this model's to fix: a local
// vector<unique_ptr<T>> frees its elements when it dies, but the vector's
// destructor invalidates the storage without running the elements' destructors,
// so the allocation still reads as unreachable. Recorded rather than hidden.
void FP_local_emplace_back_unique_ptr_ok() {
  std::vector<std::unique_ptr<T>> v;
  v.emplace_back(new T(10));
}

// KNOWN LIMITATION: unique_ptr::release() hands ownership back out, and
// dropping it is a real leak, but release() is not modelled so nothing tracks
// it. The old model did report here -- at the emplace_back line, "memory
// becomes unreachable", i.e. for the wrong reason and before the release even
// happened -- so this is not a true positive that was lost.
void FN_emplace_back_then_release_bad(std::vector<std::unique_ptr<T>>& v) {
  v.emplace_back(new T(11));
  T* p = v.back().release();
  (void)p;
}

} // namespace vector_emplace_element_ctor
