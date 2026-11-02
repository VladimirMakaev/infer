/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
package codetoanalyze.kotlin.racerd

class ContinuationHolder(var count: Int)

// kotlinc compiles a suspending function or lambda that has a suspension point into a subclass
// of kotlin.coroutines.jvm.internal.ContinuationImpl or SuspendLambda holding the state machine:
// the resumption counter `label`, the pending `result`, and a slot per value live across the
// suspension, named after its JVM type (`L$0` for a reference, `I$0` for an int). Resumptions of
// one continuation are serialised, so none of that is shared state. The locking here is what
// makes RacerD treat the generated class as thread-safe and look at its fields at all.
//
// The two shapes put the state machine in different places -- a suspending function drives it
// from the enclosing method, a suspending lambda from the generated invokeSuspend -- so both are
// covered. Only the slots are confined: what the coroutine reaches through them is still the
// program's own state.
class Continuation {

  private var guarded = 0
  private val held = ContinuationHolder(0)
  private val lock = Object()

  private suspend fun hop(x: Int): Int = x + 1

  // Taking a suspending function type is all it takes to make kotlinc generate a SuspendLambda
  // here; no coroutine library is involved.
  private fun runSuspending(block: suspend () -> Unit) = block

  // A suspending FUNCTION with two suspension points and several values live across them, so the
  // compiler needs `label` and spill slots. None of that is a race.
  suspend fun functionStateMachineOk(s: String, i: Int, h: ContinuationHolder): Int {
    val a = hop(i)
    val b = hop(a)
    synchronized(lock) { guarded = a + b }
    return b + s.length + h.count
  }

  // The same as a suspending LAMBDA, so the state machine lives in invokeSuspend instead.
  fun lambdaStateMachineOk(s: String, i: Int) {
    runSuspending {
      val a = hop(i)
      val b = hop(a)
      synchronized(lock) { guarded = a + b + s.length }
    }
  }

  // The control: a field of the enclosing class written without the lock, on the same paths that
  // carry the state machine above. A real race, and still reported.
  suspend fun writeOuterFieldWithoutLockBad(i: Int): Int {
    val a = hop(i)
    guarded = a
    return a
  }

  fun writesGuardedUnderLock(v: Int) {
    synchronized(lock) { guarded = v }
  }

  // RacerD does not report races on state a coroutine reaches from inside invokeSuspend, whether
  // through the outer-class reference `this$0` or through a spill slot. Both cases below are
  // real races and neither is reported, before this revision or after it. They are kept as
  // markers so that recovering them later reads as a gain rather than as new noise.
  fun FN_writeOuterFieldFromLambdaBad() {
    runSuspending {
      val a = hop(1)
      guarded = a
    }
  }

  suspend fun FN_writeThroughSpilledReferenceBad(): Int {
    val h = held
    val a = hop(1)
    h.count = a
    return a
  }

  fun writesHeldUnderLock(v: Int) {
    synchronized(lock) { held.count = v }
  }
}
