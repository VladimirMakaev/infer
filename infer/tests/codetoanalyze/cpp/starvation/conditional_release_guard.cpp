/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// The shape of AOSP's Mutexed<T> (media/stagefright/foundation/Mutexed.h): an accessor returns
// a guard BY VALUE, the guard holds its mutex by reference, and its destructor releases only
// when a flag says it still owns the lock. A destructor that releases on some paths and not
// others is still the releasing half of a guard; before that was recognised the class was not a
// guard at all, so every user of it ended still holding the lock -- which both hid the real
// deadlocks here and leaked the lock into callers as a phantom self deadlock.

#include <mutex>

namespace conditional_release_guard {

void work();

template <typename T>
class Mutexed {
 public:
  class Locked {
   public:
    explicit Locked(Mutexed<T>& parent)
        : mLock(parent.mLock), mTreasure(parent.mTreasure), mLocked(true) {
      mLock.lock();
    }
    ~Locked() {
      if (mLocked) {
        mLock.unlock();
      }
    }
    T* operator->() const { return &mTreasure; }
    void unlock() {
      if (mLocked) {
        mLocked = false;
        mLock.unlock();
      }
    }

   private:
    std::mutex& mLock;
    T& mTreasure;
    bool mLocked;
  };

  Locked lock() { return Locked(*this); }

 private:
  friend class Locked;
  std::mutex mLock;
  T mTreasure;
};

struct Payload {
  int slots;
};

class Channel {
 public:
  // the named guard is still alive when the temporary takes the same lock again
  int named_then_temporary_bad() {
    Mutexed<Payload>::Locked held(mInput);
    return mInput.lock()->slots;
  }

  // CONTROL. Each temporary is destroyed at the end of its own full-expression, so the lock is
  // free again before the next one takes it. The two statements matter: written as one
  // expression both temporaries would be alive at once and it would be a real self deadlock.
  int two_temporaries_ok() {
    int a = mInput.lock()->slots;
    int b = mInput.lock()->slots;
    return a + b;
  }

  int temporary_then_scoped_ok() {
    int n = mInput.lock()->slots;
    {
      Mutexed<Payload>::Locked scoped(mInput);
      n += scoped->slots;
    }
    return n;
  }

  // CONTROL. The callee takes the same lock after the caller's temporary has gone.
  int read_slots() { return mInput.lock()->slots; }

  int temporary_then_callee_ok() {
    int n = mInput.lock()->slots;
    return n + read_slots();
  }

  // CONTROL, and the reason the leak used to be invisible here: with the acquisition confined to
  // a branch the procedure can still exit balanced, so the pair survives is_balanced and the
  // leaked lock is reported. Nothing is wrong with this code.
  int branch_then_callee_ok(bool b) {
    int n = 0;
    if (b) {
      n = mInput.lock()->slots;
      n += read_slots();
    }
    return n;
  }

 private:
  Mutexed<Payload> mInput;
};

// two guards of two objects taken in opposite orders: a real inversion, invisible while neither
// side was recognised as holding anything
class Inversion {
 public:
  int forward_bad() {
    auto a = mA.lock();
    auto b = mB.lock();
    work();
    return a->slots + b->slots;
  }

  int reverse_bad() {
    auto b = mB.lock();
    auto a = mA.lock();
    work();
    return a->slots + b->slots;
  }

 private:
  Mutexed<Payload> mA, mB;
};

// CONTROL. The same two objects always in the same order: nothing to report.
class ConsistentOrder {
 public:
  int first_ok() {
    auto a = mA.lock();
    auto b = mB.lock();
    work();
    return a->slots + b->slots;
  }

  int second_ok() {
    auto a = mA.lock();
    auto b = mB.lock();
    work();
    return a->slots + b->slots;
  }

 private:
  Mutexed<Payload> mA, mB;
};

// NOT FIXED. unlock() releases the lock, so taking it again is fine, but the method is itself
// conditional and do_call_with_inferred_guards decides whether a method unlocks a guard from the
// MUST-release set, which a conditional unlock() leaves empty. The guard therefore stays locked
// and the later acquisition looks like a self deadlock. Recognising a conditional unlock() needs
// a may-release answer there too, and then the mirror for a conditional lock(), which is a
// larger change: see the notes on AE3.
class FP_ExplicitUnlock {
 public:
  int FP_unlock_then_relock_ok() {
    Mutexed<Payload>::Locked held(mInput);
    int n = held->slots;
    held.unlock();
    return n + read_slots();
  }

  int read_slots() { return mInput.lock()->slots; }

 private:
  Mutexed<Payload> mInput;
};

} // namespace conditional_release_guard
