/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
package kotlinx.coroutines.sync

import javax.annotation.concurrent.ThreadSafe

// Minimal declarations of the coroutine mutex, so the real `withLock { }` can be compiled here
// without depending on kotlinx-coroutines. The Java counterpart of this test pins the lowering
// these produce; this one pins that kotlinc really does produce it.
interface Mutex {
  val isLocked: Boolean

  suspend fun lock(owner: Any? = null)

  fun tryLock(owner: Any? = null): Boolean

  fun unlock(owner: Any? = null)
}

suspend inline fun <T> Mutex.withLock(owner: Any? = null, action: () -> T): T {
  lock(owner)
  try {
    return action()
  } finally {
    unlock(owner)
  }
}

@ThreadSafe
class CoroutineMutex(private val mutex: Mutex) {

  private var reason: String? = null
  private var counted = 0
  private var unguarded = 0

  private suspend fun hop(x: String): String = x

  suspend fun withLockWriteOk(r: String) {
    mutex.withLock { reason = r }
  }

  suspend fun withLockReadOk(): String? = mutex.withLock { reason }

  // `withLock` is inline, so a suspension INSIDE the guarded body is legal: the lock is taken
  // under one resumption label of the generated state machine and the access runs under a later
  // one. This is the shape the field report came from, and the reason the test is here and not
  // only in Java.
  suspend fun withLockSuspendingBodyOk(r: String) {
    mutex.withLock { reason = hop(r) }
  }

  suspend fun tryLockTakenOk() {
    if (mutex.tryLock()) {
      try {
        counted++
      } finally {
        mutex.unlock()
      }
    }
  }

  suspend fun tryLockReadOk(): Int = mutex.withLock { counted }

  // The model must still report what the mutex does not guard.
  fun unguardedWriteBad(n: Int) {
    unguarded = n
  }

  fun unguardedReadBad(): Int = unguarded
}
