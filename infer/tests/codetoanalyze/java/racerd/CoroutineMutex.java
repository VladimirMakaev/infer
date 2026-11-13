/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package kotlinx.coroutines.sync;

import com.facebook.infer.annotation.ThreadSafe;
import java.util.concurrent.locks.ReentrantLock;

// Minimal declaration of the coroutine mutex. `lock` is a suspend fun, so it takes a trailing
// continuation and returns Object; `withLock { }` is an inline extension function and expands at
// the call site into lock(owner, continuation) ... finally unlock(owner), which is the shape
// below and the only shape that reaches Infer.
interface Mutex {
  Object lock(Object owner, Object continuation);

  boolean tryLock(Object owner);

  void unlock(Object owner);

  boolean isLocked();

  // Omitting the defaulted `owner` at a Kotlin call site -- `mutex.unlock()` rather than
  // `mutex.unlock(owner)` -- does not call the method above. kotlinc emits these static
  // synthetic bridges on the interface, with the receiver as the first argument, and calls
  // them instead. Declared here so the Java test pins the shape the model has to match; the
  // Kotlin test next to it pins that kotlinc really emits them.
  static Object lock$default(Mutex m, Object owner, int mask, Object marker) {
    return m.lock(owner, null);
  }

  static boolean tryLock$default(Mutex m, Object owner, int mask, Object marker) {
    return m.tryLock(owner);
  }

  static void unlock$default(Mutex m, Object owner, int mask, Object marker) {
    m.unlock(owner);
  }
}

// A user extension's additional operation carries no library guarantee, exactly as for Deferred.
interface CountingMutex extends Mutex {
  void recordContention();
}

// Each field is touched by one outcome class only. Sharing one between a guarded method and an
// unguarded one makes the guarded method report as the other side of the race, which would say
// nothing about whether the mutex was understood.
@ThreadSafe
class CoroutineMutex {
  private final Mutex mutex;
  private final ReentrantLock jlock = new ReentrantLock();
  private String reason;
  private long waitMs;
  private long jdkMs;
  private int afterRelease;
  private int neverGuarded;
  private String bridged;

  CoroutineMutex(Mutex mutex) {
    this.mutex = mutex;
  }

  // What `mutex.withLock { reason = r }` expands to.
  void withLockWrite_ok(String r, Object continuation) {
    mutex.lock(null, continuation);
    try {
      reason = r;
    } finally {
      mutex.unlock(null);
    }
  }

  String withLockRead_ok(Object continuation) {
    mutex.lock(null, continuation);
    try {
      return reason;
    } finally {
      mutex.unlock(null);
    }
  }

  // tryLock returns false without acquiring, so only the taken branch is guarded.
  void tryLockTaken_ok() {
    if (mutex.tryLock(null)) {
      try {
        waitMs++;
      } finally {
        mutex.unlock(null);
      }
    }
  }

  long tryLockRead_ok(Object continuation) {
    mutex.lock(null, continuation);
    try {
      return waitMs;
    } finally {
      mutex.unlock(null);
    }
  }

  // The same shape with a java.util.concurrent lock, which was already silent: the model must
  // not have changed anything for the locks that worked before it.
  void jdkLock_ok(long ms) {
    jlock.lock();
    try {
      jdkMs += ms;
    } finally {
      jlock.unlock();
    }
  }

  long jdkLockRead_ok() {
    jlock.lock();
    try {
      return jdkMs;
    } finally {
      jlock.unlock();
    }
  }

  // The point of the model is that it still reports what the mutex does not guard.
  void neverGuardedWrite_bad(int n) {
    neverGuarded = n;
  }

  int neverGuardedRead_bad() {
    return neverGuarded;
  }

  void afterUnlock_bad(Object continuation) {
    mutex.lock(null, continuation);
    mutex.unlock(null);
    afterRelease++;
  }

  int afterUnlockRead_bad() {
    return afterRelease;
  }

  // Controls for the interface-call half: an operation of the declaration is covered, a user
  // extension's own operation is not.
  boolean isLocked_ok() {
    return mutex.isLocked();
  }

  void userExtension_bad(CountingMutex counting) {
    counting.recordContention();
  }

  // The same guarded access reached through the defaulted-argument bridges.
  void defaultedBridge_ok(String r, Object continuation) {
    Mutex.lock$default(mutex, null, 1, null);
    try {
      bridged = r;
    } finally {
      Mutex.unlock$default(mutex, null, 1, null);
    }
  }

  String defaultedBridgeRead_ok() {
    Mutex.lock$default(mutex, null, 1, null);
    try {
      return bridged;
    } finally {
      Mutex.unlock$default(mutex, null, 1, null);
    }
  }
}
