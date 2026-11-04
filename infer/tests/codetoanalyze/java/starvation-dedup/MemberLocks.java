/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

class MemberLocksOwner {
  final Object mFirst = new Object();
  final Object mSecond = new Object();

  // the deadlocks of the clients with this method are only found from the clients, as the paths of
  // these locks do not go through the client classes, and are reported whether the name of the
  // client class comes before or after this one
  void lockFirstThenSecond() {
    synchronized (mFirst) {
      synchronized (mSecond) {
      }
    }
  }
}

class AMemberLocksClient {
  MemberLocksOwner mOwner;

  void lockSecondThenFirstBad() {
    synchronized (mOwner.mSecond) {
      synchronized (mOwner.mFirst) {
      }
    }
  }
}

class ZMemberLocksClient {
  MemberLocksOwner mOwner;

  void lockSecondThenFirstBad() {
    synchronized (mOwner.mSecond) {
      synchronized (mOwner.mFirst) {
      }
    }
  }
}

// Known FN; see the parent/child ordering follow-up. Both sides form a cyclic inversion.
// The MemberLocksA/MemberLocksB deadlock is found from both sides but reported once
class MemberLocksA {
  MemberLocksB b;
  private final Object mLock = new Object();

  void FN_lockThenCallBBad() { // Known FN; see the parent/child ordering follow-up.
    synchronized (mLock) {
      b.lockB();
    }
  }

  void lockA() {
    synchronized (mLock) {
    }
  }
}

class MemberLocksB {
  MemberLocksA a;
  private final Object mLock = new Object();

  void lockB() {
    synchronized (mLock) {
    }
  }

  void FN_lockThenCallABad() { // Known FN; see the parent/child ordering follow-up.
    synchronized (mLock) {
      a.lockA();
    }
  }
}
