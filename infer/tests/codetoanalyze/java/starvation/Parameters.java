/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

class Parameters {
  private static void syncOnParam(Object x) {
    synchronized (x) {
    }
  }

  // Next two methods will deadlock
  public synchronized void oneWaySyncOnParamBad(Object x) {
    syncOnParam(x);
  }

  public void otherWaySyncOnParamBad(Object x) {
    synchronized (x) {
      synchronized (this) {
      }
    }
  }

  private static void emulateSynchronized(Parameters self) {
    synchronized (self) {
    }
  }

  Parameters someObject;

  // Next two methods will deadlock
  public synchronized void oneWayEmulateSyncBad() {
    emulateSynchronized(someObject);
  }

  public void anotherWayEmulateSyncBad() {
    synchronized (someObject) {
      synchronized (this) {
      }
    }
  }

  static Parameters sObject;
  static final Object sLock = new Object();

  // Next two methods will deadlock; the first one has no parameters
  static void staticEmulateSyncBad() {
    synchronized (sLock) {
      emulateSynchronized(sObject);
    }
  }

  void staticObjectThenLockBad() {
    synchronized (sObject) {
      synchronized (sLock) {
      }
    }
  }

  final java.util.concurrent.locks.Lock mLock = new java.util.concurrent.locks.ReentrantLock();

  // A second instance lock and static lock. A deadlock is grouped per canonical lock pair, so
  // the four methods below that invert an instance lock against a static one would otherwise
  // be one row and only the pair's two ends would stay asserted. Split across two pairs, each
  // pair keeps one method on each side, so every one of the four is named in a row and losing
  // any of them takes its row away.
  final java.util.concurrent.locks.Lock mLock2 = new java.util.concurrent.locks.ReentrantLock();
  static final Object sLock2 = new Object();

  static Parameters getInstance() {
    return sObject;
  }

  void acquire() {
    mLock.lock();
  }

  void release() {
    mLock.unlock();
  }

  void acquire2() {
    mLock2.lock();
  }

  void release2() {
    mLock2.unlock();
  }

  // the next three methods deadlock if the instance is sObject
  static void instanceThenStaticLockBad() {
    getInstance().acquire();
    synchronized (sLock) {
    }
    getInstance().release();
  }

  static void localInstanceThenStaticLockBad() {
    Parameters p = getInstance();
    p.acquire2();
    synchronized (sLock2) {
    }
    p.release2();
  }

  void staticLockThenInstanceBad() {
    synchronized (sLock) {
      mLock.lock();
      mLock.unlock();
    }
  }

  static void acquireAndRelease(Parameters p) {
    p.acquire2();
    p.release2();
  }

  static void staticLockThenHelperBad() {
    synchronized (sLock2) {
      acquireAndRelease(getInstance());
    }
  }

  synchronized void syncMethod() {}

  void thisThenStaticLockOk() {
    synchronized (this) {
      synchronized (sLock) {
      }
    }
  }

  // a fresh object cannot be locked by another thread
  static void staticLockThenFreshOk() {
    Parameters p = new Parameters();
    synchronized (sLock) {
      p.syncMethod();
    }
  }
}
