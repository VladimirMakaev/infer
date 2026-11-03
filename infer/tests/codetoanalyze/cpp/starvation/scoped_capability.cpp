/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// A lock that is not std::mutex but a class marked as a capability, taken through a guard marked
// as scoped_lockable whose constructor is DEFINED. The constructor says what it acquires in its
// attribute; its body says something less useful -- it builds a std::unique_lock over a member,
// which becomes a guard, and guarded locks do not survive into a summary. Before the attribute
// was believed for such a constructor the guard acquired nothing at all and every inversion
// through it was invisible.
//
// The controls are the point: believing the attribute must not merge locks that are distinct,
// must not lose the self deadlock that naming the wrapper instead of the mutex inside it would
// otherwise hide, and must still respect a wrapper around a recursive mutex.

#include <mutex>

#define TSA(x) __attribute__((x))

namespace scoped_capability {

template <class M>
class TSA(capability("mutex")) Wrapper {
 public:
  using MutexType = M;
  void lock() TSA(acquire_capability()) { m_.lock(); }
  void unlock() TSA(release_capability()) { m_.unlock(); }
  M m_;
};

using Mutex = Wrapper<std::mutex>;
using RecursiveMutex = Wrapper<std::recursive_mutex>;

// The guard: its constructor is defined, and only the attribute says what it takes.
template <class W>
class TSA(scoped_lockable) Guard {
 public:
  explicit Guard(W& w) TSA(acquire_capability(w, wrapped_mutex())) : lock_(w.m_) {}
  ~Guard() TSA(release_capability()) = default;
  [[nodiscard]] const W* wrapped_mutex() const noexcept; // for the attribute only
 private:
  std::unique_lock<typename W::MutexType> lock_;
};

void work();

// Two locks of one object taken in opposite orders: a real inversion, and the report this whole
// change exists to restore.
class Inversion {
 public:
  // both sides of an inversion are reported, as elsewhere in this suite
  void forward_bad() {
    Guard<Mutex> a(mConn);
    Guard<Mutex> b(mBuf);
    work();
  }

  void reverse_bad() {
    Guard<Mutex> a(mBuf);
    Guard<Mutex> b(mConn);
    work();
  }

 private:
  Mutex mConn, mBuf;
};

// CONTROL. The same two locks always in the same order: nothing to report. If the guard's
// acquisitions were merged into one lock this would become a self deadlock.
class ConsistentOrder {
 public:
  void first_ok() {
    Guard<Mutex> a(mConn);
    Guard<Mutex> b(mBuf);
    work();
  }

  void second_ok() {
    Guard<Mutex> a(mConn);
    Guard<Mutex> b(mBuf);
    work();
  }

 private:
  Mutex mConn, mBuf;
};

// CONTROL. Another class whose fields happen to be taken in the opposite order. Its locks are
// not Inversion's locks, so the two must not be paired.
class Unrelated {
 public:
  void start_ok() {
    Guard<Mutex> a(mBuf);
    Guard<Mutex> b(mConn);
    work();
  }

  void stop_ok() {
    Guard<Mutex> a(mBuf);
    Guard<Mutex> b(mConn);
    work();
  }

 private:
  Mutex mConn, mBuf;
};

// CONTROL, and the one the recursion half exists for. The guard now names the WRAPPER rather
// than the std::mutex inside it; a type with no lock model is assumed recursive so as not to
// invent self deadlocks, so without inheriting the wrapped mutex's recursion this report would
// quietly disappear.
class SelfDeadlock {
 public:
  void twice_bad() {
    Guard<Mutex> a(mLock);
    Guard<Mutex> b(mLock);
    work();
  }

 private:
  Mutex mLock;
};

// CONTROL. The same shape around a RECURSIVE mutex: taking it twice is legal, and the recursion
// must be inherited from the wrapped mutex rather than assumed either way.
class RecursiveSelf {
 public:
  void twice_ok() {
    Guard<RecursiveMutex> a(mLock);
    Guard<RecursiveMutex> b(mLock);
    work();
  }

 private:
  RecursiveMutex mLock;
};

} // namespace scoped_capability
