/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mutex>
#define TSA(x) __attribute__((x))

namespace callfree_assertion {
int counter;
std::mutex member_mutex;
void noop() {}
void lock_mutex(std::mutex& mutex) { mutex.lock(); }

class TSA(scoped_lockable) MemberAssertion {
 public:
  explicit MemberAssertion(std::mutex& mutex) TSA(acquire_capability(mutex)) {
    count_ = 0;
  }
  ~MemberAssertion() TSA(release_capability()) {}
 private:
  int count_;
};

class TSA(scoped_lockable) BranchAssertion {
 public:
  explicit BranchAssertion(std::mutex& mutex) TSA(acquire_capability(mutex)) {
    bool local = true;
    if (local) {}
  }
  ~BranchAssertion() TSA(release_capability()) {}
};

class TSA(scoped_lockable) GlobalAssertion {
 public:
  explicit GlobalAssertion(std::mutex& mutex) TSA(acquire_capability(mutex)) {
    counter = 0;
  }
  ~GlobalAssertion() TSA(release_capability()) {}
};

// Keep the contract when a helper call may have a locking effect.
class TSA(scoped_lockable) HelperAssertion {
 public:
  explicit HelperAssertion(std::mutex& mutex) TSA(acquire_capability(mutex)) { noop(); }
  ~HelperAssertion() TSA(release_capability()) {}
};

class TSA(scoped_lockable) DefaultMemberGuard {
 public:
  explicit DefaultMemberGuard(std::mutex& mutex) TSA(acquire_capability(mutex)) {}
  ~DefaultMemberGuard() TSA(release_capability()) = default;
 private:
  std::unique_lock<std::mutex> held_{member_mutex};
};

class TSA(scoped_lockable) MemberGuard {
 public:
  explicit MemberGuard(std::mutex& mutex) TSA(acquire_capability(mutex)) : held_(mutex) {}
  ~MemberGuard() TSA(release_capability()) = default;
 private:
  std::unique_lock<std::mutex> held_;
};

class TSA(scoped_lockable) BaseGuard : public MemberGuard {
 public:
  explicit BaseGuard(std::mutex& mutex) TSA(acquire_capability(mutex)) : MemberGuard(mutex) {}
  ~BaseGuard() TSA(release_capability()) = default;
};

class TSA(scoped_lockable) IndirectGuard {
 public:
  using Locker = void (*)(std::mutex&);
  explicit IndirectGuard(std::mutex& mutex, Locker locker) TSA(acquire_capability(mutex)) {
    locker(mutex);
  }
  ~IndirectGuard() TSA(release_capability()) {}
};

void member_store_assertion_ok(std::mutex& mutex) {
  std::unique_lock<std::mutex> held(mutex);
  MemberAssertion assertion(mutex);
}
void local_branch_assertion_ok(std::mutex& mutex) {
  std::unique_lock<std::mutex> held(mutex);
  BranchAssertion assertion(mutex);
}
void global_store_assertion_ok(std::mutex& mutex) {
  std::unique_lock<std::mutex> held(mutex);
  GlobalAssertion assertion(mutex);
}
// Known conservative false positive: a constructor containing a call keeps its contract.
void FP_noop_call_assertion_ok(std::mutex& mutex) {
  std::unique_lock<std::mutex> held(mutex);
  HelperAssertion assertion(mutex);
}
void member_guard_bad(std::mutex& mutex) {
  std::unique_lock<std::mutex> held(mutex);
  MemberGuard second(mutex);
}
void default_member_guard_bad() {
  std::unique_lock<std::mutex> held(member_mutex);
  DefaultMemberGuard second(member_mutex);
}
void base_guard_bad(std::mutex& mutex) {
  std::unique_lock<std::mutex> held(mutex);
  BaseGuard second(mutex);
}
void indirect_guard_bad(std::mutex& mutex) {
  std::unique_lock<std::mutex> held(mutex);
  IndirectGuard second(mutex, lock_mutex);
}
// Atomic builtins may lower to loads/stores, so no Call does not prove assertion-only.
class TSA(capability("mutex")) SpinMutex {
 public:
  unsigned char flag_ = 0;
};
class TSA(scoped_lockable) SpinGuard {
 public:
  explicit SpinGuard(SpinMutex& mutex) TSA(acquire_capability(mutex)) : mutex_(mutex) {
    while (__atomic_test_and_set(&mutex.flag_, __ATOMIC_ACQUIRE)) {}
  }
  ~SpinGuard() TSA(release_capability()) {
    __atomic_clear(&mutex_.flag_, __ATOMIC_RELEASE);
  }
 private:
  SpinMutex& mutex_;
};
class SpinOwner {
 public:
  void forward_bad() {
    SpinGuard first(first_);
    SpinGuard second(second_);
  }
  void reverse_bad() {
    SpinGuard first(second_);
    SpinGuard second(first_);
  }
 private:
  SpinMutex first_;
  SpinMutex second_;
};
// Local initializers and assignments can absorb the AtomicExpr node kind.
class TSA(scoped_lockable) NestedSpinGuard {
 public:
  explicit NestedSpinGuard(SpinMutex& mutex) TSA(acquire_capability(mutex)) : mutex_(mutex) {
    bool taken = __atomic_test_and_set(&mutex.flag_, __ATOMIC_ACQUIRE);
    while (taken) {
      taken = __atomic_test_and_set(&mutex.flag_, __ATOMIC_ACQUIRE);
    }
  }
  ~NestedSpinGuard() TSA(release_capability()) {
    __atomic_clear(&mutex_.flag_, __ATOMIC_RELEASE);
  }
 private:
  SpinMutex& mutex_;
};
class NestedSpinOwner {
 public:
  void forward_bad() {
    NestedSpinGuard first(first_);
    NestedSpinGuard second(second_);
  }
  void reverse_bad() {
    NestedSpinGuard first(second_);
    NestedSpinGuard second(first_);
  }
 private:
  SpinMutex first_;
  SpinMutex second_;
};
} // namespace callfree_assertion
