/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// See dedup_always.h. Everything below is checked with --no-deduplicate.

#include "dedup_always.h"

namespace dedup_always {

// One pair of locks is one report, whichever procedure and whichever order
// reaches it. Four procedures and both orders here, one row expected.
class OnePairManyProcedures {
 public:
  void first_then_second_bad() {
    std::lock_guard<std::mutex> a(first_);
    std::lock_guard<std::mutex> b(second_);
  }
  void first_then_second_again_bad() {
    std::lock_guard<std::mutex> a(first_);
    std::lock_guard<std::mutex> b(second_);
  }
  void second_then_first_bad() {
    std::lock_guard<std::mutex> b(second_);
    std::lock_guard<std::mutex> a(first_);
  }
  void second_then_first_again_bad() {
    std::lock_guard<std::mutex> b(second_);
    std::lock_guard<std::mutex> a(first_);
  }

 private:
  std::mutex first_;
  std::mutex second_;
};

// THE CONTROL THE GROUPING MUST NOT SWALLOW: a second, unrelated pair in the
// same file. If the grouping ever keyed on the file alone this row would go.
class ADifferentPair {
 public:
  void third_then_fourth_bad() {
    std::lock_guard<std::mutex> c(third_);
    std::lock_guard<std::mutex> d(fourth_);
  }
  void fourth_then_third_bad() {
    std::lock_guard<std::mutex> d(fourth_);
    std::lock_guard<std::mutex> c(third_);
  }

 private:
  std::mutex third_;
  std::mutex fourth_;
};

// A self deadlock and an inversion that both start at the lock_guard in
// drain_bad are two root causes, so the key carries the kind of deadlock and
// not only the pair. Two rows, one of each kind. They are at different
// procedures here only because the option is off: with it on, one side of an
// inversion is dropped before this point, and without it both sides survive
// and the grouping keeps the earliest, which is begin_bad.
class SelfAndInversionOnOneLine {
 public:
  int begin_bad(int id) {
    std::lock_guard<std::recursive_mutex> lock(state_);
    add_entry(id);
    return 0;
  }
  int finish_bad(int id, bool remove) {
    std::lock_guard<std::recursive_mutex> lock(state_);
    if (remove) {
      remove_entry(id);
    }
    return 0;
  }
  int drain_bad(int id) {
    std::lock_guard<std::mutex> lock(entries_);
    return finish_bad(id, false);
  }

 private:
  void add_entry(int id) { std::lock_guard<std::mutex> lock(entries_); }
  void remove_entry(int id) { std::lock_guard<std::mutex> lock(entries_); }
  std::recursive_mutex state_;
  std::mutex entries_;
};

// A canonical name says which field deadlocks, not which object, so an
// inversion between two objects of one field has the pair (lock_, lock_) --
// what a self deadlock on that field has. Both are real and both must report.
struct Element {
  std::mutex lock_;
};

class TwoObjectsOneField {
 public:
  void forward_bad() {
    std::lock_guard<std::mutex> g(left_.lock_);
    std::lock_guard<std::mutex> h(right_.lock_);
  }
  void reverse_bad() {
    std::lock_guard<std::mutex> g(right_.lock_);
    std::lock_guard<std::mutex> h(left_.lock_);
  }

 private:
  Element left_;
  Element right_;
};

class SelfOnTheSameField {
 public:
  void retake_bad() {
    std::lock_guard<std::mutex> g(only_.lock_);
    retake();
  }

 private:
  void retake() { std::lock_guard<std::mutex> g(only_.lock_); }
  Element only_;
};

// A caller that takes no lock of its own repeats its callee's report. It stays
// quiet when the two callees share a file, and must NOT when they do not.
class JoinedHelpers {
 public:
  void gamma_then_delta_bad() {
    std::lock_guard<std::mutex> g(gamma_);
    std::lock_guard<std::mutex> d(delta_);
  }
  void delta_then_gamma_bad() {
    std::lock_guard<std::mutex> d(delta_);
    std::lock_guard<std::mutex> g(gamma_);
  }

 private:
  std::mutex gamma_;
  std::mutex delta_;
};

class JoinedService {
 public:
  // both callees are in this file, so the inversion is reported between them
  void forward_ok() { helpers_->gamma_then_delta_bad(); }
  void reverse_ok() { helpers_->delta_then_gamma_bad(); }

 private:
  JoinedHelpers* helpers_;
};

class SplitService {
 public:
  // the callees are in dedup_always_alpha.cpp and dedup_always_beta.cpp, which
  // never see each other, so this is the only place the inversion can appear
  void forward_bad() { split_alpha_then_beta_bad(); }
  void reverse_bad() { split_beta_then_alpha_bad(); }
};

void drive(JoinedService* joined, SplitService* split) {
  joined->forward_ok();
  joined->reverse_ok();
  split->forward_bad();
  split->reverse_bad();
}

} // namespace dedup_always
