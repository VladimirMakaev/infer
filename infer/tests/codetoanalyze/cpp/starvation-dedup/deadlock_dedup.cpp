/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// One deadlock is one unordered pair of canonical locks. These classes reach the
// same pair from both sides, through several callers and under several spellings
// of the same field, and each must be reported exactly once.

#include <mutex>

namespace deadlock_dedup {

// Both orders of the same two fields, each from several methods. Every spelling
// below names the fields declared here, so all of it is one report.
class TwoLocks {
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

  std::mutex first_;
  std::mutex second_;
};

// The same two locks spelled through a pointer to the owner instead of through
// `this`. The canonical name is the declaring class and field either way, so this
// adds no second report.
class ReachesTwoLocksThroughPointer {
 public:
  void through_pointer_bad() {
    std::lock_guard<std::mutex> a(owner_->first_);
    std::lock_guard<std::mutex> b(owner_->second_);
  }

  TwoLocks* owner_;
};

// A nested object holding the same owner reaches the fields by a longer path.
class NestedHolder {
 public:
  void nested_bad() {
    std::lock_guard<std::mutex> b(inner_.owner_->second_);
    std::lock_guard<std::mutex> a(inner_.owner_->first_);
  }

 private:
  ReachesTwoLocksThroughPointer inner_;
};

// A public wrapper that only forwards. Both acquisitions come from a callee that
// reports them itself, so the wrapper must not repeat the report.
class ForwardingService {
 public:
  void start_ok() { tracker_->first_then_second_bad(); }
  void stop_ok() { tracker_->second_then_first_bad(); }

 private:
  TwoLocks* tracker_;
};

// An inversion and a self deadlock REPORTED AT THE SAME LINE. Both must survive:
// keying on the lock pair is what keeps them apart, while keying on the location
// merges them into whichever has the shorter trace and silently drops the other.
//
// `drain_bad` holds entries_ at the lock_guard below, then calls `finish_bad`,
// which takes state_ in the opposite order from `begin_bad` (the inversion) and
// may take entries_ again through `remove_entry` (the self deadlock). Both
// reports anchor on that one lock_guard line.
class InversionAndSelfOnOneLine {
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

// A self deadlock reached only through a public caller: the lock that is held is
// taken by `hold_then_retake_bad` itself, so that is where it is reported, and
// not at either caller below.
class SelfDeadlockHasOneHome {
 public:
  ~SelfDeadlockHasOneHome() { hold_then_retake_bad(); }
  void hold_then_retake_bad() {
    std::lock_guard<std::mutex> lock(mutex_);
    retake();
  }
  void restart_ok() { hold_then_retake_bad(); }

 private:
  void retake() { std::lock_guard<std::mutex> lock(mutex_); }
  std::mutex mutex_;
};

// The control against over-suppressing callers. `ensure_` is PRIVATE, so it never
// reports its own inversion; the only place it can be reported is a caller. If
// caller suppression ignored whether the callee is reportable, this pair would be
// lost entirely rather than deduplicated.
class PrivateCalleeStillReported {
 public:
  int read_bad() {
    ensure();
    return state_;
  }
  int peek_bad() {
    ensure();
    return state_;
  }
  void enable_bad() {
    std::lock_guard<std::recursive_mutex> a(recursive_);
    ensure();
  }

 private:
  void ensure() {
    std::lock_guard<std::mutex> g(guard_);
    std::lock_guard<std::recursive_mutex> a(recursive_);
    state_ = 1;
  }

  std::recursive_mutex recursive_;
  std::mutex guard_;
  int state_ = 0;
};

// Two genuinely different pairs in one class must stay two reports: canonicalising
// must not merge distinct locks.
class TwoDistinctPairs {
 public:
  void ab_bad() {
    std::lock_guard<std::mutex> x(a_);
    std::lock_guard<std::mutex> y(b_);
  }
  void ba_bad() {
    std::lock_guard<std::mutex> y(b_);
    std::lock_guard<std::mutex> x(a_);
  }
  void cd_bad() {
    std::lock_guard<std::mutex> x(c_);
    std::lock_guard<std::mutex> y(d_);
  }
  void dc_bad() {
    std::lock_guard<std::mutex> y(d_);
    std::lock_guard<std::mutex> x(c_);
  }

 private:
  std::mutex a_;
  std::mutex b_;
  std::mutex c_;
  std::mutex d_;
};

// A canonical name says which FIELD deadlocks, not which object. Two different
// objects of one class therefore have the same canonical lock, and an inversion
// between them is the pair (Element::lock_, Element::lock_) -- exactly the pair a
// self deadlock on that field has. They are different bugs, so the report is
// deduplicated on the KIND of deadlock as well as on the pair. The next two
// classes are that collision: both must report.
class Element {
 public:
  std::mutex lock_;
  int value_ = 0;
};

// A real lock-order inversion. left_ and right_ are two distinct mutexes.
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

// A real self deadlock on that same field, in that same file. Keyed on the pair
// alone this one is the report that disappears.
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

// The textbook shape: a method that locks its own object and then another object
// of the same class, called with the two arguments swapped. Every call site
// inverts Account::mutex_ against itself, so they are one root cause and the
// canonical key keeps ONE report for them -- at the earliest site, naming the
// concrete objects of that site rather than the canonical field twice.
class Account {
 public:
  void transfer_bad(Account& other) {
    std::lock_guard<std::mutex> mine(mutex_);
    std::lock_guard<std::mutex> theirs(other.mutex_);
    balance_ -= 1;
  }

  std::mutex mutex_;
  int balance_ = 0;
};

class Teller {
 public:
  void pay_bad() { first_.transfer_bad(second_); }
  void refund_bad() { second_.transfer_bad(first_); }
  void settle_bad() { second_.transfer_bad(first_); }

 private:
  Account first_;
  Account second_;
};

} // namespace deadlock_dedup
