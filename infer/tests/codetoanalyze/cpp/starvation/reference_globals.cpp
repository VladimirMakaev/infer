/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mutex>

// A lock taken through a reference-typed global is the referent's lock. Before this was
// resolved the reference variable became a lock of its own, so a lock order spelled through
// it could never meet the same mutex locked directly and the inversion was invisible.
//
// Every pair below gets its own mutexes on purpose: a deadlock is grouped per canonical lock,
// so sharing one would merge two rows into one and stop asserting the second.

namespace reference_globals {

int x;

std::mutex ref_mutex_1;
std::mutex ref_mutex_2;
std::mutex& ref_to_mutex_2 = ref_mutex_2;

// deadlock between ref_1_then_alias_2_bad() and plain_2_then_1_bad(): the second acquisition
// names ref_mutex_2 through a reference and is the same lock
void ref_1_then_alias_2_bad() {
  std::lock_guard<std::mutex> lock1(ref_mutex_1);
  std::lock_guard<std::mutex> lock2(ref_to_mutex_2);
  x++;
}

void plain_2_then_1_bad() {
  std::lock_guard<std::mutex> lock2(ref_mutex_2);
  std::lock_guard<std::mutex> lock1(ref_mutex_1);
  x--;
}

std::mutex chained_mutex_1;
std::mutex chained_mutex_2;
std::mutex& first_ref_to_chained_2 = chained_mutex_2;
std::mutex& second_ref_to_chained_2 = first_ref_to_chained_2;

// the same deadlock reached through a reference bound to another reference: both spellings
// have to land on chained_mutex_2
void chained_1_then_alias_2_bad() {
  std::lock_guard<std::mutex> lock1(chained_mutex_1);
  std::lock_guard<std::mutex> lock2(second_ref_to_chained_2);
  x++;
}

void chained_2_then_1_bad() {
  std::lock_guard<std::mutex> lock2(chained_mutex_2);
  std::lock_guard<std::mutex> lock1(chained_mutex_1);
  x--;
}

std::mutex self_ref_mutex;
std::mutex& ref_to_self_ref_mutex = self_ref_mutex;

// a self deadlock needs no second thread, and is the sharpest test of identity alone: it
// does not depend on pairing or on thread reasoning
void self_deadlock_through_reference_bad() {
  std::lock_guard<std::mutex> lock(self_ref_mutex);
  ref_to_self_ref_mutex.lock();
  x++;
  ref_to_self_ref_mutex.unlock();
}

std::mutex distinct_mutex_1;
std::mutex distinct_mutex_2;
std::mutex& ref_to_distinct_1 = distinct_mutex_1;
std::mutex& ref_to_distinct_2 = distinct_mutex_2;

// THE CONTROL THAT CATCHES OVER-RESOLUTION. If resolving a reference collapsed distinct
// mutexes onto one address, holding both of these at once would read as a self deadlock.
// Its silence is what says the two references stayed apart.
void lock_two_distinct_through_references_ok() {
  std::lock_guard<std::mutex> lock1(ref_to_distinct_1);
  std::lock_guard<std::mutex> lock2(ref_to_distinct_2);
  x++;
}

// the ordinary control: resolved or not, one order everywhere is not a deadlock
void distinct_1_then_2_ok() {
  std::lock_guard<std::mutex> lock1(ref_to_distinct_1);
  std::lock_guard<std::mutex> lock2(distinct_mutex_2);
  x++;
}

void distinct_1_then_2_again_ok() {
  std::lock_guard<std::mutex> lock1(distinct_mutex_1);
  std::lock_guard<std::mutex> lock2(ref_to_distinct_2);
  x--;
}

std::mutex pointer_mutex_1;
std::mutex pointer_mutex_2;
std::mutex* pointer_to_mutex_2 = &pointer_mutex_2;

// NOT a reference, and deliberately left alone. A pointer can be assigned again after its
// initialiser runs, so what it points to where the lock is taken does not follow from how it
// was initialised. Resolving it would be a guess, and a wrong one merges two unrelated locks
// and invents a deadlock. The missed report is the price of not doing that.
void FN_pointer_1_then_2_bad() {
  std::lock_guard<std::mutex> lock1(pointer_mutex_1);
  std::lock_guard<std::mutex> lock2(*pointer_to_mutex_2);
  x++;
}

void FN_pointer_2_then_1_bad() {
  std::lock_guard<std::mutex> lock2(pointer_mutex_2);
  std::lock_guard<std::mutex> lock1(pointer_mutex_1);
  x--;
}

std::mutex static_local_mutex_1;
std::mutex static_local_mutex_2;

// NOT covered, and not for want of a resolver: the frontend emits no store and no initialiser
// procedure for a function-local static reference, so the binding is absent from the IR
// entirely and nothing downstream can recover it. Fixing it belongs in the clang frontend.
void FN_static_local_ref_1_then_2_bad() {
  static std::mutex& ref_to_static_local_2 = static_local_mutex_2;
  std::lock_guard<std::mutex> lock1(static_local_mutex_1);
  std::lock_guard<std::mutex> lock2(ref_to_static_local_2);
  x++;
}

void FN_static_local_2_then_1_bad() {
  std::lock_guard<std::mutex> lock2(static_local_mutex_2);
  std::lock_guard<std::mutex> lock1(static_local_mutex_1);
  x--;
}

std::mutex member_ref_mutex_1;
std::mutex member_ref_mutex_2;

struct HoldsAReference {
  std::mutex& held_;
  explicit HoldsAReference(std::mutex& mutex) : held_(mutex) {}
  void lock_held() { std::lock_guard<std::mutex> lock(held_); }
};

HoldsAReference holder(member_ref_mutex_2);

// NOT covered by this commit. The lock is rooted at the formal `this` with a field access, and
// the field is bound to the constructor's parameter, which is only tied to member_ref_mutex_2
// at the construction site. That is two hops where a global reference is one, so it needs its
// own resolution step rather than this one widened.
void FN_member_ref_1_then_held_bad() {
  std::lock_guard<std::mutex> lock1(member_ref_mutex_1);
  holder.lock_held();
}

void FN_member_ref_2_then_1_bad() {
  std::lock_guard<std::mutex> lock2(member_ref_mutex_2);
  std::lock_guard<std::mutex> lock1(member_ref_mutex_1);
  x--;
}

} // namespace reference_globals
