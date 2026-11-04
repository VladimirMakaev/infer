/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

struct FakeMut {
  int blah;
};
void pthread_mutex_lock(struct FakeMut*);
void pthread_mutex_unlock(struct FakeMut*);

struct MemberB;

struct MemberA {
  struct FakeMut mutex;
  struct MemberB* b;
};

struct MemberB {
  struct FakeMut mutex;
  struct MemberA* a;
};

// Cyclic owner back-pointers can deadlock; parent/child ordering currently misses this case.
// Without this known false negative, deduplication reports the inversion only once.
void FN_lock_a_then_b_bad(struct MemberA* a) { // Known FN; see the parent/child ordering follow-up.
  pthread_mutex_lock(&a->mutex);
  pthread_mutex_lock(&a->b->mutex);
  pthread_mutex_unlock(&a->b->mutex);
  pthread_mutex_unlock(&a->mutex);
}

void FN_lock_b_then_a_bad(struct MemberB* b) { // Known FN; see the parent/child ordering follow-up.
  pthread_mutex_lock(&b->mutex);
  pthread_mutex_lock(&b->a->mutex);
  pthread_mutex_unlock(&b->a->mutex);
  pthread_mutex_unlock(&b->mutex);
}
