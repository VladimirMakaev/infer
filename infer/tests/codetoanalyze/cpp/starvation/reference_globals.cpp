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

// A BLOCK-LOCAL reference is bound by a single store in its own procedure, so it denotes the
// same mutex at every lock below it. Before this the variable was not a formal and had no
// address, so the acquisition was dropped outright and no order through it was ever seen.

std::mutex block_local_mutex_1;
std::mutex block_local_mutex_2;

// deadlock between block_local_ref_1_then_alias_2_bad() and block_local_plain_2_then_1_bad()
void block_local_ref_1_then_alias_2_bad() {
  std::lock_guard<std::mutex> lock1(block_local_mutex_1);
  std::mutex& local_ref = block_local_mutex_2;
  std::lock_guard<std::mutex> lock2(local_ref);
  x++;
}

void block_local_plain_2_then_1_bad() {
  std::lock_guard<std::mutex> lock2(block_local_mutex_2);
  std::lock_guard<std::mutex> lock1(block_local_mutex_1);
  x--;
}

std::mutex block_local_chained_1;
std::mutex block_local_chained_2;

// the same, reached through a block-local reference bound to another block-local reference
void block_local_chained_1_then_alias_2_bad() {
  std::lock_guard<std::mutex> lock1(block_local_chained_1);
  std::mutex& first = block_local_chained_2;
  std::mutex& second = first;
  std::lock_guard<std::mutex> lock2(second);
  x++;
}

void block_local_chained_plain_2_then_1_bad() {
  std::lock_guard<std::mutex> lock2(block_local_chained_2);
  std::lock_guard<std::mutex> lock1(block_local_chained_1);
  x--;
}

std::mutex self_local_mutex;

// locking the same mutex twice, once directly and once through a block-local reference
void self_deadlock_through_block_local_ref_bad() {
  std::lock_guard<std::mutex> lock1(self_local_mutex);
  std::mutex& local_ref = self_local_mutex;
  std::lock_guard<std::mutex> lock2(local_ref);
  x++;
}

std::mutex distinct_local_1;
std::mutex distinct_local_2;

// OVER-RESOLUTION CONTROL: two block-local references to two DIFFERENT mutexes, locked in the
// same order by both functions. If the two references collapsed onto one address this would
// read as a self deadlock, and if they were merged with each other it would read as a pair.
void lock_two_distinct_through_block_local_refs_ok() {
  std::mutex& ref_a = distinct_local_1;
  std::mutex& ref_b = distinct_local_2;
  std::lock_guard<std::mutex> lock1(ref_a);
  std::lock_guard<std::mutex> lock2(ref_b);
  x++;
}

void lock_two_distinct_directly_ok() {
  std::lock_guard<std::mutex> lock1(distinct_local_1);
  std::lock_guard<std::mutex> lock2(distinct_local_2);
  x++;
}

std::mutex ambiguous_local_1;
std::mutex ambiguous_local_2;
bool pick_local();

// AMBIGUOUS CONTROL: bound by one of two stores, so neither can be named without guessing.
// The acquisition must stay unresolved rather than pick one and invent an order.
void ambiguous_block_local_ref_ok() {
  std::mutex& amb = pick_local() ? ambiguous_local_1 : ambiguous_local_2;
  std::lock_guard<std::mutex> lock1(amb);
  std::lock_guard<std::mutex> lock2(ambiguous_local_1);
  x++;
}

void ambiguous_plain_1_then_2_ok() {
  std::lock_guard<std::mutex> lock1(ambiguous_local_1);
  std::lock_guard<std::mutex> lock2(ambiguous_local_2);
  x--;
}

std::mutex pointer_local_1;
std::mutex pointer_local_2;

// POINTER CONTROL: a pointer can be reassigned after its initialiser, so what it points to
// where the lock is taken does not follow from how it was initialised. Still dropped.
void FN_block_local_pointer_1_then_2_bad() {
  std::lock_guard<std::mutex> lock1(pointer_local_1);
  std::mutex* ptr = &pointer_local_2;
  std::lock_guard<std::mutex> lock2(*ptr);
  x++;
}

void FN_block_local_pointer_plain_2_then_1_bad() {
  std::lock_guard<std::mutex> lock2(pointer_local_2);
  std::lock_guard<std::mutex> lock1(pointer_local_1);
  x--;
}

// NOT covered. A reference bound to a LOCAL object never reaches a global, and the domain has
// no address rooted at a local, so there is nothing to name it with.
void FN_ref_to_local_object_self_deadlock_bad() {
  std::mutex local_mutex;
  std::mutex& local_ref = local_mutex;
  std::lock_guard<std::mutex> lock1(local_mutex);
  std::lock_guard<std::mutex> lock2(local_ref);
  x++;
}

std::mutex shared_after_ref;

std::mutex sibling_scope_1;
std::mutex sibling_scope_2;

// Two references with the SAME SOURCE NAME in sibling scopes are two distinct variables, each
// bound once. Resolving them per variable keeps them apart; keying the binding on anything
// name-shaped would let one of the two stores win and silently rename the other's lock. Both
// orders below are real, so each partner gets its own row and neither may go missing.
void block_local_same_name_sibling_scopes_bad(bool c) {
  if (c) {
    std::mutex& r = sibling_scope_1;
    std::lock_guard<std::mutex> lock1(r);
    std::lock_guard<std::mutex> lock2(shared_after_ref);
  } else {
    std::mutex& r = sibling_scope_2;
    std::lock_guard<std::mutex> lock1(r);
    std::lock_guard<std::mutex> lock2(shared_after_ref);
  }
  x++;
}

void block_local_sibling_partner_1_bad() {
  std::lock_guard<std::mutex> lock2(shared_after_ref);
  std::lock_guard<std::mutex> lock1(sibling_scope_1);
  x--;
}

void block_local_sibling_partner_2_bad() {
  std::lock_guard<std::mutex> lock2(shared_after_ref);
  std::lock_guard<std::mutex> lock1(sibling_scope_2);
  x--;
}

// A block-local reference may be bound to a global that is ITSELF a reference. Resolving only
// as far as that global would name the reference variable, and since its type is still a
// reference it would be resolved again later and acquire a SECOND address-of, spelling a lock
// nobody else spells. The chain is followed to the object at its end instead, so exactly one
// address is taken and these meet the same mutex locked directly.

std::mutex chain_target_1;
std::mutex chain_target_2;
std::mutex& chain_global_ref_2 = chain_target_2;

// D2: a global reference locked directly, against the same mutex reached through a local
// reference bound to that global reference.
void chain_global_ref_1_then_local_bad() {
  std::lock_guard<std::mutex> lock1(chain_target_1);
  std::mutex& local_ref = chain_global_ref_2;
  std::lock_guard<std::mutex> lock2(local_ref);
  x++;
}

void chain_global_ref_plain_2_then_1_bad() {
  std::lock_guard<std::mutex> lock2(chain_global_ref_2);
  std::lock_guard<std::mutex> lock1(chain_target_1);
  x--;
}

std::mutex chain_direct_1;
std::mutex chain_direct_2;
std::mutex& chain_direct_ref_2 = chain_direct_2;

// D3: the REFERENT locked directly, against the same mutex reached through a local reference
// bound to the global reference. This is the pair the double address-of hid.
void chain_direct_1_then_local_bad() {
  std::lock_guard<std::mutex> lock1(chain_direct_1);
  std::mutex& local_ref = chain_direct_ref_2;
  std::lock_guard<std::mutex> lock2(local_ref);
  x++;
}

void chain_direct_referent_2_then_1_bad() {
  std::lock_guard<std::mutex> lock2(chain_direct_2);
  std::lock_guard<std::mutex> lock1(chain_direct_1);
  x--;
}

std::mutex pointer_global_target;
std::mutex* pointer_global = &pointer_global_target;
std::mutex pointer_global_other;

// CONTROL: the chain must NOT run through a POINTER global. A pointer can be assigned again
// after its initialiser, so what it points to where the lock is taken does not follow from how
// it was initialised. Taking the address of a variable and of a pointer variable look the same
// in the IR, so only the binding's type keeps these apart; if that check is ever lost this pair
// starts reporting.
void FN_pointer_global_deref_1_then_2_bad() {
  std::lock_guard<std::mutex> lock1(pointer_global_other);
  std::mutex& local_ref = *pointer_global;
  std::lock_guard<std::mutex> lock2(local_ref);
  x++;
}

void FN_pointer_global_plain_2_then_1_bad() {
  std::lock_guard<std::mutex> lock2(pointer_global_target);
  std::lock_guard<std::mutex> lock1(pointer_global_other);
  x--;
}

std::mutex pointer_mediated_target;
std::mutex pointer_mediated_other;
std::mutex* pointer_mediated_ptr = &pointer_mediated_target;
std::mutex& pointer_mediated_ref = *pointer_mediated_ptr;

// The same refusal as above, with the reference global locked DIRECTLY rather than through a
// local reference bound to it. `pointer_mediated_ref` cannot be rebound, but it is bound by
// reading a pointer global, and nothing settles what that pointer held when this initialiser
// ran: another translation unit's dynamic initialiser may assign it first, and the order of
// those is not fixed. Naming a referent here would mean guessing, and a wrong guess merges two
// unrelated locks into one invented deadlock, so this inversion is given up on by design.
void FN_pointer_mediated_ref_1_then_2_bad() {
  std::lock_guard<std::mutex> lock1(pointer_mediated_other);
  std::lock_guard<std::mutex> lock2(pointer_mediated_ref);
  x++;
}

void FN_pointer_mediated_referent_2_then_1_bad() {
  std::lock_guard<std::mutex> lock2(pointer_mediated_target);
  std::lock_guard<std::mutex> lock1(pointer_mediated_other);
  x--;
}

} // namespace reference_globals
