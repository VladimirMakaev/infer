/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
package codetoanalyze.kotlin.racerd

// A Kotlin local `var` captured by a lambda is compiled to a kotlin.jvm.internal.Ref$* cell,
// allocated in the enclosing scope and held in a field of the generated lambda class named after
// the source variable: `$first` for `var first`. Reading or writing the variable from the lambda
// becomes an access to `this.$first.element`.
//
// No coroutine library is needed to generate these: a suspending function TYPE is enough to make
// kotlinc emit a SuspendLambda, which is what Continuation.kt in this directory already relies on.
// `collectS` stands in for Flow.collect and `runSuspending` for a launch.
class RefCapture {

  private var last: String? = null
  private val lock = Object()

  private suspend fun hop(x: Int): Int = x + 1

  private fun runSuspending(block: suspend () -> Unit) = block

  private suspend fun collectS(items: List<String>, f: suspend (String) -> Unit) {
    for (i in items) f(i)
  }

  private fun each(items: List<String>, f: (String) -> Unit) {
    for (i in items) f(i)
  }

  // The reported shape: the var lives in an enclosing suspending function and is read and written
  // by one inner suspending lambda, which also takes a lock. One cell, one lambda, invocations
  // serialised by the collect loop.
  suspend fun outerSuspendFunInnerCollectOk(items: List<String>) {
    var first = true
    collectS(items) { d ->
      if (first) {
        first = false
      }
      synchronized(lock) { last = d }
    }
  }

  // The same with the enclosing scope a suspending lambda rather than a function, so the cell is
  // allocated inside another generated class.
  fun outerSuspendLambdaInnerCollectOk(items: List<String>) {
    runSuspending {
      var first = true
      collectS(items) { d ->
        if (first) {
          first = false
        }
        synchronized(lock) { last = d }
      }
    }
  }

  // CONTROL: no lock anywhere. Nothing makes the generated class look concurrent, so this is
  // silent before the change as well -- it pins which ingredient does the work, and would catch a
  // fix that worked by making the whole file uninteresting.
  suspend fun noLockOk(items: List<String>) {
    var first = true
    collectS(items) { d ->
      if (first) {
        first = false
      }
      println(d)
    }
  }

  // CONTROL: the lock is in the enclosing scope instead of the lambda. Also silent before.
  suspend fun lockInOuterScopeOk(items: List<String>) {
    var first = true
    synchronized(lock) { last = null }
    collectS(items) { d ->
      if (first) {
        first = false
      }
    }
  }

  // CONTROL: a plain, non-suspending inner lambda holding the same kind of cell. Silent before,
  // so the FP needs the inner lambda to be a continuation and not merely a lambda.
  suspend fun plainInnerLambdaOk(items: List<String>) {
    val a = hop(1)
    var first = true
    each(items) { d ->
      if (first) {
        first = false
      }
      synchronized(lock) { last = d + a }
    }
  }

  // CONTROL, true positive: an ordinary field of the real class written without the lock from a
  // suspending function. The enclosing class is not generated code, so this must stay reported.
  // If it ever goes silent the change has stopped RacerD looking at suspending code at all.
  suspend fun writeOuterFieldWithoutLockBad(s: String): Int {
    val a = hop(1)
    last = s
    return a
  }

  fun writesLastUnderLock(v: String) {
    synchronized(lock) { last = v }
  }

  // CONTROL, true positive, and the one that bounds how far this goes: a captured real OBJECT
  // mutated from the lambda. A Holder is the program's own state, not a compiler cell, so only
  // the cell is confined and an access made THROUGH a capture is still reported.
  class Holder(var count: Int)

  fun capturedObjectMutatedBad(h: Holder): suspend () -> Unit {
    return runSuspending {
      h.count += 1
      synchronized(lock) { last = "h" }
    }
  }

  // THE COST, recorded rather than left out of the suite.
  //
  // One cell really can be shared between two concurrent contexts, and then the race is real.
  // These two were reported before this revision and are not reported after it, because at the
  // lambda they are indistinguishable from the confined case above: both are "a continuation
  // writes a captured cell unprotected". What separates them -- how many lambdas hold the cell,
  // whether the enclosing scope still writes it -- is known only at the creation site, and there
  // is no call edge from there to invokeSuspend for it to travel along.
  //
  // What makes that acceptable rather than a silent loss of coverage: the same two races written
  // with a plain lambda are missed today and were missed before this revision, because kotlinc
  // passes the cell as a parameter there. The detection was an artefact of the suspending
  // lowering, not an analysis result. Recovering it needs the creation site, which is where it
  // should have been reported in the first place.
  fun FN_twoLambdasShareOneRefBad(): Pair<suspend () -> Unit, suspend () -> Unit> {
    var shared = 0
    val a = runSuspending {
      shared += 1
      synchronized(lock) { last = "a" }
    }
    val b = runSuspending {
      shared += 2
      synchronized(lock) { last = "b" }
    }
    return Pair(a, b)
  }

  fun FN_enclosingAlsoWritesRefBad(): suspend () -> Unit {
    var shared = 0
    val a = runSuspending {
      shared += 1
      synchronized(lock) { last = "a" }
    }
    shared += 100
    return a
  }
}
