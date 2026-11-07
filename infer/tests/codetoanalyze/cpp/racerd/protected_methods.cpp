/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace protected_methods {

class AllCallersLocked {
 public:
  void set(int x) {
    std::lock_guard<std::mutex> lock(mutex_);
    set_locked_ok(x);
  }

  void reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    set_locked_ok(0);
  }

  int get() {
    std::lock_guard<std::mutex> lock(mutex_);
    return x_;
  }

 protected:
  void set_locked_ok(int x) {
    last_ = x_;
    x_ = x;
  }

 private:
  std::mutex mutex_;
  int x_;
  int last_;
};

class OneCallerUnlocked {
 public:
  void set(int x) {
    std::lock_guard<std::mutex> lock(mutex_);
    set_locked_bad(x);
  }

  void set_unlocked_bad(int x) { set_locked_bad(x); }

 protected:
  void set_locked_bad(int x) {
    last_ = x_;
    x_ = x;
  }

 private:
  std::mutex mutex_;
  int x_;
  int last_;
};

class NoCallerInClass {
 public:
  void set(int x) {
    std::lock_guard<std::mutex> lock(mutex_);
    x_ = x;
  }

 protected:
  int get_bad() { return x_; }

 private:
  std::mutex mutex_;
  int x_;
};

class PublicHelper {
 public:
  void set(int x) {
    std::lock_guard<std::mutex> lock(mutex_);
    set_locked_bad(x);
  }

  void set_locked_bad(int x) {
    last_ = x_;
    x_ = x;
  }

 private:
  std::mutex mutex_;
  int x_;
  int last_;
};

class Base {
 public:
  void set(int x) {
    std::lock_guard<std::mutex> lock(mutex_);
    set_locked_bad(x);
  }

 protected:
  // An unlocked derived caller must prevent treating this helper as private.
  void set_locked_bad(int x) {
    last_ = x_;
    x_ = x;
  }

 private:
  std::mutex mutex_;
  int x_;
  int last_;
};

class Derived : public Base {
 public:
  void set_unlocked(int x) { set_locked_bad(x); }
};

class LockedDerived : public AllCallersLocked {
 public:
  void set_guarded_ok(int x) { set(x); }
};

class IndirectBase {
 public:
  void set(int x) {
    std::lock_guard<std::mutex> lock(mutex_);
    set_locked_bad(x);
  }

 protected:
  void set_locked_bad(int x) {
    last_ = x_;
    x_ = x;
  }

 private:
  std::mutex mutex_;
  int x_;
  int last_;
};

class IndirectDerived : public IndirectBase {
 public:
  void set_unlocked_bad(int x) { through_private(x); }

 private:
  void through_private(int x) { set_locked_bad(x); }
};

class ContractBase {
 public:
  void set(int x) {
    std::lock_guard<std::mutex> lock(mutex_);
    set_locked_ok(x);
  }

 protected:
  void set_locked_ok(int x) {
    last_ = x_;
    x_ = x;
  }
  std::mutex mutex_;

 private:
  int x_;
  int last_;
};

class ContractDerived : public ContractBase {
 public:
  void set_requires_lock_ok(int x)
      __attribute__((requires_capability(mutex_))) {
    set_locked_ok(x);
  }
};

// A protected helper reached only through ANOTHER protected helper. The caller's lock was
// credited for one hop and lost at the second, so the inner helper reported. It is locked
// exactly when its only caller is, which is a question that cannot be settled until the caller
// has been.
class ChainedCallers {
 public:
  void set(int x) {
    std::lock_guard<std::mutex> lock(mutex_);
    outer_ok(x);
  }

 protected:
  void outer_ok(int x) { inner_ok(x); }
  void inner_ok(int x) {
    last_ = x_;
    x_ = x;
  }

 private:
  std::mutex mutex_;
  int x_;
  int last_;
};

// The chain's true-positive control: the middle helper also has a caller holding no lock, so
// neither it nor the helper below it may be credited. One unlocked caller still spoils the
// whole chain, not just its first link.
class ChainedOneCallerUnlocked {
 public:
  void set(int x) {
    std::lock_guard<std::mutex> lock(mutex_);
    outer_bad(x);
  }

  void set_unlocked_bad(int x) { outer_bad(x); }

 protected:
  void outer_bad(int x) { inner_bad(x); }
  void inner_bad(int x) {
    last_ = x_;
    x_ = x;
  }

 private:
  std::mutex mutex_;
  int x_;
  int last_;
};

// Two protected helpers that call each other and nothing else calls either under a lock. They
// cannot vouch for one another: the credit is a least fixpoint, so neither is ever the one that
// justifies the other. A greatest fixpoint would silence both.
class MutuallyRecursiveHelpers {
 public:
  void set(int x) {
    std::lock_guard<std::mutex> lock(mutex_);
    x_ = x;
  }

  void entry_unlocked_bad(int x) { ping_bad(x); }

 protected:
  void ping_bad(int x) {
    if (x > 0) {
      pong_bad(x - 1);
    }
    last_ = x_;
  }
  void pong_bad(int x) { ping_bad(x); }

 private:
  std::mutex mutex_;
  int x_;
  int last_;
};


// A protected helper that UNLOCKS around its call to another protected helper. The chain credit
// must not reach the inner helper: the credit means "my callers hold the lock throughout my body",
// and a body that releases it around the call is precisely where that stops being true.
//
// The outer lock state at the call is what decides this, not the state on entry. The caller here
// IS entered with the lock held; it has simply given it back by the time it calls inner_bad. Note
// the second, unrelated mutex: without it the lock COUNT drops to zero at the call and the inner
// helper is denied credit for the wrong reason, so the bug hides. Found by an independent verifier
// on exactly this shape.
class ReleaseThenCallUnderOtherLock {
 public:
  void write(int x) {
    std::lock_guard<std::mutex> guard(mu_);
    value_ = x;
  }
  int read() {
    std::lock_guard<std::mutex> guard(mu_);
    std::lock_guard<std::mutex> unrelated(extra_);
    return outer_bad();
  }

 protected:
  int outer_bad() {
    mu_.unlock();
    int x = inner_bad();
    mu_.lock();
    return x;
  }
  int inner_bad() { return value_; }

 private:
  std::mutex mu_;
  std::mutex extra_;
  int value_ = 0;
};

// The same release, but the unrelated lock is taken by the OUTER helper itself, so at the call the
// caller both holds a lock and has released one. Kept as its own fixture because it probes a
// different part of the state: a count cannot say WHICH lock survived, and the normalisation that
// records a locked access can drop the release flag.
class ReleaseWhileHoldingOther {
 public:
  void write(int x) {
    std::lock_guard<std::mutex> guard(mu_);
    value_ = x;
  }
  int read() {
    std::lock_guard<std::mutex> guard(mu_);
    return outer_bad();
  }

 protected:
  int outer_bad() {
    std::lock_guard<std::mutex> unrelated(extra_);
    mu_.unlock();
    int x = inner_bad();
    mu_.lock();
    return x;
  }
  int inner_bad() { return value_; }

 private:
  std::mutex mu_;
  std::mutex extra_;
  int value_ = 0;
};


// The release can be DELEGATED. outer_bad has no lock instruction of its own; it calls a private
// helper that unlocks. The evidence that a lock was touched therefore arrives through the callee's
// summary, and if it is not joined there, outer_bad looks like a pure forwarder and hands the
// chain credit to inner_bad, dropping a real race.
class DelegatedRelease {
 public:
  void write(int x) {
    std::lock_guard<std::mutex> guard(mu_);
    value_ = x;
  }
  int read() {
    std::lock_guard<std::mutex> guard(mu_);
    std::lock_guard<std::mutex> unrelated(extra_);
    return outer_bad();
  }

 protected:
  int outer_bad() {
    release();
    int x = inner_bad();
    reacquire();
    return x;
  }
  int inner_bad() { return value_; }

 private:
  void release() { mu_.unlock(); }
  void reacquire() { mu_.lock(); }
  std::mutex mu_;
  std::mutex extra_;
  int value_ = 0;
};

// The same, two hops down. A join that only looked one level deep would pass this.
class DelegatedReleaseTwice {
 public:
  void write(int x) {
    std::lock_guard<std::mutex> guard(mu_);
    value_ = x;
  }
  int read() {
    std::lock_guard<std::mutex> guard(mu_);
    std::lock_guard<std::mutex> unrelated(extra_);
    return outer_bad();
  }

 protected:
  int outer_bad() {
    release_hop();
    int x = inner_bad();
    reacquire_hop();
    return x;
  }
  int inner_bad() { return value_; }

 private:
  void release_hop() { release(); }
  void reacquire_hop() { reacquire(); }
  void release() { mu_.unlock(); }
  void reacquire() { mu_.lock(); }
  std::mutex mu_;
  std::mutex extra_;
  int value_ = 0;
};

// The release is scoped by a hand-written RAII type, so the unlock happens in a DESTRUCTOR. The
// destructor's summary is projected down to its lock effect alone, and the lock-touch evidence has
// to survive that projection or this reads as a forwarder too.
class UnlockOnDestroy {
 public:
  explicit UnlockOnDestroy(std::mutex* mutex) : mutex_(mutex) {}
  ~UnlockOnDestroy() { mutex_->unlock(); }

 private:
  std::mutex* mutex_;
};

class DestructorRelease {
 public:
  void write(int x) {
    std::lock_guard<std::mutex> guard(mu_);
    value_ = x;
  }
  int read() {
    std::lock_guard<std::mutex> guard(mu_);
    std::lock_guard<std::mutex> unrelated(extra_);
    int x = outer_bad();
    mu_.lock();
    return x;
  }

 protected:
  int outer_bad() {
    { UnlockOnDestroy release(&mu_); }
    return inner_bad();
  }
  int inner_bad() { return value_; }

 private:
  std::mutex mu_;
  std::mutex extra_;
  int value_ = 0;
};

// The safe twin of the three above, and the reason the rule is not simply "a helper that touches a
// lock never vouches". Here the lock is given back and RETAKEN before the call, so it is held at
// the call site and the inner helper is credited directly, without the chain clause being
// consulted at all. It must stay silent: a fix that made all four of these report would be
// over-correcting.
class ReacquiredBeforeRead {
 public:
  void write(int x) {
    std::lock_guard<std::mutex> guard(mu_);
    value_ = x;
  }
  int read() {
    std::lock_guard<std::mutex> guard(mu_);
    std::lock_guard<std::mutex> unrelated(extra_);
    return outer();
  }

 protected:
  int outer() {
    release();
    reacquire();
    return inner_ok();
  }
  int inner_ok() { return value_; }

 private:
  void release() { mu_.unlock(); }
  void reacquire() { mu_.lock(); }
  std::mutex mu_;
  std::mutex extra_;
  int value_ = 0;
};


// The lock disturbance can arrive through a call the analyser cannot resolve. In each of the four
// below, outer_bad has no lock instruction and no resolvable callee that has one, so there is no
// OBSERVED effect -- but absence of an observed effect is not proof of absence, and the chain
// credit was reading it as proof. An unresolved call now counts as "may touch".

struct AbstractUnlocker {
  virtual ~AbstractUnlocker() = default;
  virtual void release(std::mutex& m) = 0;
  virtual void reacquire(std::mutex& m) = 0;
};

void pointer_release(std::mutex& m) { m.unlock(); }
void pointer_reacquire(std::mutex& m) { m.lock(); }

// Erased through a std::function held in a local.
class FunctionBothLocal {
 public:
  void write(int x) {
    std::lock_guard<std::mutex> g(mu_);
    value_ = x;
  }
  int read() {
    std::lock_guard<std::mutex> g(mu_);
    std::lock_guard<std::mutex> e(extra_);
    std::function<void()> r = [this] { mu_.unlock(); };
    std::function<void()> a = [this] { mu_.lock(); };
    return outer_bad(r, a);
  }

 protected:
  int outer_bad(const std::function<void()>& r, const std::function<void()>& a) {
    r();
    int x = inner_bad();
    a();
    return x;
  }
  int inner_bad() { return value_; }

 private:
  std::mutex mu_;
  std::mutex extra_;
  int value_ = 0;
};

// Erased through a std::function held in a member, so the call target is not even local.
class FunctionBothMember {
 public:
  void write(int x) {
    std::lock_guard<std::mutex> g(mu_);
    value_ = x;
  }
  int read() {
    std::lock_guard<std::mutex> g(mu_);
    std::lock_guard<std::mutex> e(extra_);
    release_ = [this] { mu_.unlock(); };
    reacquire_ = [this] { mu_.lock(); };
    return outer_bad();
  }

 protected:
  int outer_bad() {
    release_();
    int x = inner_bad();
    reacquire_();
    return x;
  }
  int inner_bad() { return value_; }

 private:
  std::mutex mu_;
  std::mutex extra_;
  int value_ = 0;
  std::function<void()> release_;
  std::function<void()> reacquire_;
};

// Dispatched through an abstract base, so there is no body to analyse at the call.
class VirtualAbstract {
 public:
  void write(int x) {
    std::lock_guard<std::mutex> g(mu_);
    value_ = x;
  }
  int read(AbstractUnlocker& u) {
    std::lock_guard<std::mutex> g(mu_);
    std::lock_guard<std::mutex> e(extra_);
    return outer_bad(u);
  }

 protected:
  int outer_bad(AbstractUnlocker& u) {
    u.release(mu_);
    int x = inner_bad();
    u.reacquire(mu_);
    return x;
  }
  int inner_bad() { return value_; }

 private:
  std::mutex mu_;
  std::mutex extra_;
  int value_ = 0;
};

// Reached through a plain function pointer.
class FunctionPointer {
 public:
  void write(int x) {
    std::lock_guard<std::mutex> g(mu_);
    value_ = x;
  }
  int read() {
    std::lock_guard<std::mutex> g(mu_);
    std::lock_guard<std::mutex> e(extra_);
    return outer_bad(pointer_release, pointer_reacquire);
  }

 protected:
  int outer_bad(void (*r)(std::mutex&), void (*a)(std::mutex&)) {
    r(mu_);
    int x = inner_bad();
    a(mu_);
    return x;
  }
  int inner_bad() { return value_; }

 private:
  std::mutex mu_;
  std::mutex extra_;
  int value_ = 0;
};

// A container model stands for the container's own bookkeeping, not for the
// element code it runs. Each of these reaches a lock through a user-defined
// member that a model would otherwise hide: an element's assignment operator,
// its copy or default constructor, or a key's comparison operator.

// An element whose assignment touches a lock.
struct AssignBox {
  std::mutex* mutex = nullptr;
  bool unlock = false;
  AssignBox& operator=(const AssignBox& o) {
    if (o.mutex) {
      if (o.unlock)
        o.mutex->unlock();
      else
        o.mutex->lock();
    }
    mutex = o.mutex;
    unlock = o.unlock;
    return *this;
  }
};

// How many times a container invokes an element's or a key's user-defined
// member is an implementation detail: a copy constructor may run again when the
// buffer is reallocated, a resize constructs as many elements as it grows by,
// and a lookup compares as often as the tree is deep -- or, on an EMPTY
// container, not at all. Unlocking directly inside such a member is therefore
// not a balanced release: it may happen twice, or never.
//
// So arm one pending action and let the first invocation consume it. The net
// effect is exactly one release, or exactly one reacquire, however many times
// the callback runs, and the containers below are populated and their capacity
// reserved so that at least one call is guaranteed.
std::mutex* pending_mutex = nullptr;
int pending_action = 0; // 0 = none, 1 = release, 2 = reacquire

void arm_release(std::mutex* m) {
  pending_mutex = m;
  pending_action = 1;
}

void arm_reacquire(std::mutex* m) {
  pending_mutex = m;
  pending_action = 2;
}

void run_pending_action() {
  const int action = pending_action;
  pending_action = 0;
  if (action == 1)
    pending_mutex->unlock();
  else if (action == 2)
    pending_mutex->lock();
}

// An element whose copy constructor touches a lock. The user-declared copy
// constructor also suppresses the implicit move, so pushing a temporary copies.
struct CopyBox {
  int k = 0;
  CopyBox() = default;
  CopyBox(const CopyBox& o) : k(o.k) { run_pending_action(); }
  CopyBox& operator=(const CopyBox&) = default;
};

// An element whose default constructor touches a lock.
struct CtorBox {
  CtorBox() { run_pending_action(); }
};

// A key whose comparison touches a lock. A lookup in a NON-EMPTY ordered
// container compares at least once.
struct ComparingKey {
  int k = 0;
  ComparingKey() = default;
  explicit ComparingKey(int key) : k(key) {}
  bool operator<(const ComparingKey& o) const {
    run_pending_action();
    return k < o.k;
  }
};

// Assigning a vector invokes the element's operator=.
class VectorAssignElement {
 public:
  VectorAssignElement() : target_(1), release_(1), reacquire_(1) {
    release_[0].mutex = &mu_;
    release_[0].unlock = true;
    reacquire_[0].mutex = &mu_;
    reacquire_[0].unlock = false;
  }
  void write(int x) {
    std::lock_guard<std::mutex> g(mu_);
    value_ = x;
  }
  int read() {
    std::lock_guard<std::mutex> g(mu_);
    std::lock_guard<std::mutex> e(extra_);
    return outer_bad();
  }

 protected:
  int outer_bad() {
    target_ = release_;
    int x = inner_bad();
    target_ = reacquire_;
    return x;
  }
  int inner_bad() { return value_; }

 private:
  std::mutex mu_;
  std::mutex extra_;
  int value_ = 0;
  std::vector<AssignBox> target_, release_, reacquire_;
};

// push_back invokes the element's copy constructor. Capacity is reserved up
// front, so no push reallocates and re-copies an element already stored.
class VectorPushCopyCtor {
 public:
  VectorPushCopyCtor() { v_.reserve(4); }
  void write(int x) {
    std::lock_guard<std::mutex> g(mu_);
    value_ = x;
  }
  int read() {
    std::lock_guard<std::mutex> g(mu_);
    std::lock_guard<std::mutex> e(extra_);
    return outer_bad();
  }

 protected:
  int outer_bad() {
    arm_release(&mu_);
    v_.push_back(CopyBox());
    int x = inner_bad();
    arm_reacquire(&mu_);
    v_.push_back(CopyBox());
    return x;
  }
  int inner_bad() { return value_; }

 private:
  std::mutex mu_;
  std::mutex extra_;
  int value_ = 0;
  std::vector<CopyBox> v_;
};

// resize invokes the element's default constructor, once per element it grows
// by, so each phase here grows by exactly one.
class VectorResizeCtor {
 public:
  VectorResizeCtor() { grown_.reserve(4); }
  void write(int x) {
    std::lock_guard<std::mutex> g(mu_);
    value_ = x;
  }
  int read() {
    std::lock_guard<std::mutex> g(mu_);
    std::lock_guard<std::mutex> e(extra_);
    return outer_bad();
  }

 protected:
  int outer_bad() {
    arm_release(&mu_);
    grown_.resize(1);
    int x = inner_bad();
    arm_reacquire(&mu_);
    grown_.resize(2);
    return x;
  }
  int inner_bad() { return value_; }

 private:
  std::mutex mu_;
  std::mutex extra_;
  int value_ = 0;
  std::vector<CtorBox> grown_;
};

// erase shifts the surviving elements with the element's operator=.
class VectorEraseShift {
 public:
  VectorEraseShift() : v_(3), w_(3) {
    v_[1].mutex = &mu_;
    v_[1].unlock = true;
    w_[1].mutex = &mu_;
    w_[1].unlock = false;
  }
  void write(int x) {
    std::lock_guard<std::mutex> g(mu_);
    value_ = x;
  }
  int read() {
    std::lock_guard<std::mutex> g(mu_);
    std::lock_guard<std::mutex> e(extra_);
    return outer_bad();
  }

 protected:
  int outer_bad() {
    v_.erase(v_.begin());
    int x = inner_bad();
    w_.erase(w_.begin());
    return x;
  }
  int inner_bad() { return value_; }

 private:
  std::mutex mu_;
  std::mutex extra_;
  int value_ = 0;
  std::vector<AssignBox> v_, w_;
};

// The subscript operator of a map is a container WRITE, and it invokes the
// key's operator<.
class MapSubscriptComparator {
 public:
  MapSubscriptComparator() {
    m_[ComparingKey(1)] = 1;
    m_[ComparingKey(2)] = 2;
  }
  void write(int x) {
    std::lock_guard<std::mutex> g(mu_);
    value_ = x;
  }
  int read() {
    std::lock_guard<std::mutex> g(mu_);
    std::lock_guard<std::mutex> e(extra_);
    return outer_bad();
  }

 protected:
  int outer_bad() {
    arm_release(&mu_);
    m_[ComparingKey(1)] = 3;
    int x = inner_bad();
    arm_reacquire(&mu_);
    m_[ComparingKey(2)] = 4;
    return x;
  }
  int inner_bad() { return value_; }

 private:
  std::mutex mu_;
  std::mutex extra_;
  int value_ = 0;
  std::map<ComparingKey, int> m_;
};

// find is a container READ, and it also invokes the key's operator<. This is
// the read half of the same hole.
class MapFindComparator {
 public:
  MapFindComparator() {
    m_[ComparingKey(1)] = 1;
    m_[ComparingKey(2)] = 2;
  }
  void write(int x) {
    std::lock_guard<std::mutex> g(mu_);
    value_ = x;
  }
  int read() {
    std::lock_guard<std::mutex> g(mu_);
    std::lock_guard<std::mutex> e(extra_);
    return outer_bad();
  }

 protected:
  int outer_bad() {
    arm_release(&mu_);
    m_.find(ComparingKey(1));
    int x = inner_bad();
    arm_reacquire(&mu_);
    m_.find(ComparingKey(2));
    return x;
  }
  int inner_bad() { return value_; }

 private:
  std::mutex mu_;
  std::mutex extra_;
  int value_ = 0;
  std::map<ComparingKey, int> m_;
};

// Any operator[], on any class, is modelled as a container read -- including a
// hand-written one on a type that is not a container at all. The model then
// stands for a body that is right there in the source.
struct SubscriptHolder {
  std::mutex* mutex = nullptr;
  bool unlock = false;
  int slot = 0;
  int& operator[](int i) {
    if (mutex) {
      if (unlock)
        mutex->unlock();
      else
        mutex->lock();
    }
    return slot;
  }
};
class UserSubscriptRead {
 public:
  UserSubscriptRead() {
    release_.mutex = &mu_;
    release_.unlock = true;
    reacquire_.mutex = &mu_;
    reacquire_.unlock = false;
  }
  void write(int x) {
    std::lock_guard<std::mutex> g(mu_);
    value_ = x;
  }
  int read() {
    std::lock_guard<std::mutex> g(mu_);
    std::lock_guard<std::mutex> e(extra_);
    return outer_bad();
  }

 protected:
  int outer_bad() {
    release_[0] = 1;
    int x = inner_bad();
    reacquire_[0] = 1;
    return x;
  }
  int inner_bad() { return value_; }

 private:
  std::mutex mu_;
  std::mutex extra_;
  int value_ = 0;
  SubscriptHolder release_, reacquire_;
};

// The control for consulting the callee's summary rather than marking every
// modelled call. This hand-written subscript operator is also modelled as a
// container read, but unlike SubscriptHolder it is DEFINED, so there is a
// summary saying it touches no lock and the chain credit survives. Marking
// unconditionally would report inner_ok here.
struct QuietHolder {
  int slot = 0;
  int& operator[](int i) { return slot; }
};
class UserSubscriptNoLock {
 public:
  void write(int x) {
    std::lock_guard<std::mutex> g(mu_);
    value_ = x;
  }
  int read() {
    std::lock_guard<std::mutex> g(mu_);
    return outer();
  }

 protected:
  int outer() {
    h_[0] = 1;
    return inner_ok();
  }
  int inner_ok() { return value_; }

 private:
  std::mutex mu_;
  int value_ = 0;
  QuietHolder h_;
};

// The boundary of the credit. These containers hold plain types and are ordered
// by a built-in comparison, so nothing they call can touch a lock -- but the
// standard library is captured without bodies, so no summary says so and the
// credit is withheld. The base reports this too: not a regression, an unfixed FP.
class PlainContainersInChain {
 public:
  void write(int x) {
    std::lock_guard<std::mutex> g(mu_);
    value_ = x;
  }
  int read() {
    std::lock_guard<std::mutex> g(mu_);
    return outer();
  }

 protected:
  int outer() {
    ints_.push_back(1);
    counts_.find(1);
    text_.push_back('x');
    return FP_inner_ok();
  }
  // Known FP. Reported on the base arm as well, so this change does not add it;
  // it is the case the credit cannot reach, not one it breaks. A user-defined
  // container IS defined, and UserSubscriptNoLock above keeps its credit.
  int FP_inner_ok() { return value_; }

 private:
  std::mutex mu_;
  int value_ = 0;
  std::vector<int> ints_;
  std::map<int, int> counts_;
  std::string text_;
};

} // namespace protected_methods
