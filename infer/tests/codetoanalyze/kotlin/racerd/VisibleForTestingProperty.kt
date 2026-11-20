/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package codetoanalyze.kotlin.racerd

import com.facebook.infer.annotation.ThreadSafe
import com.google.common.annotations.VisibleForTesting as PropertyTargetVisibleForTesting
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock

// Where an unqualified property annotation ends up in the bytecode depends on the annotation's
// own targets, and the two placements are mutually exclusive. Guava's annotation declares no
// @Target, so PROPERTY is among its targets and kotlinc puts it on a synthetic static
// getClock$annotations() method, leaving the backing field bare. The one below has FIELD but not
// PROPERTY, which is androidx.annotation.VisibleForTesting's target set; kotlinc puts that one on
// the backing field. It is declared here rather than taken from a jar, the same way
// CoroutineMutex.kt declares a minimal Mutex: that keeps the suite self-contained, and between
// the two every placement the fix has to look at is pinned, so a kotlinc change that moves one
// fails here instead of in a report months later.
@Target(
    AnnotationTarget.CLASS,
    AnnotationTarget.FUNCTION,
    AnnotationTarget.CONSTRUCTOR,
    AnnotationTarget.FIELD)
@Retention(AnnotationRetention.BINARY)
annotation class VisibleForTesting

class FieldAnnotatedProperty(private val mutex: Mutex) {
  @VisibleForTesting var clock: Long = 0L

  // not annotated: the true-positive control for the same shape in the same class
  var other: Long = 0L

  private var last = 0L

  suspend fun readTestOnlyPropertyOk() {
    val n = clock
    mutex.withLock { last = n }
  }

  suspend fun readOtherPropertyBad() {
    val n = other
    mutex.withLock { last = n }
  }
}

class SyntheticAnnotatedProperty(private val mutex: Mutex) {
  @PropertyTargetVisibleForTesting var clock: Long = 0L

  var other: Long = 0L

  private var last = 0L

  suspend fun readTestOnlyPropertyOk() {
    val n = clock
    mutex.withLock { last = n }
  }

  suspend fun readOtherPropertyBad() {
    val n = other
    mutex.withLock { last = n }
  }
}

// The annotation excuses the generated accessors, not the field. A production method of the class
// assigns the backing field directly rather than going through the setter, so that write is a
// production write and the race with it is still reported.
class WritesBackingFieldDirectly(private val mutex: Mutex) {
  @VisibleForTesting var clock: Long = 0L

  private var last = 0L

  fun resetFromProduction() {
    clock = 0L
  }

  suspend fun readBad() {
    val n = clock
    mutex.withLock { last = n }
  }
}

// A Boolean property's getter is isReady, not getReady, so its synthetic is isReady$annotations
// while its setter is still setReady. Nothing else in the suite pins that spelling.
class BooleanProperty(private val mutex: Mutex) {
  @PropertyTargetVisibleForTesting var isReady: Boolean = false

  private var last = false

  suspend fun readTestOnlyPropertyOk() {
    val n = isReady
    mutex.withLock { last = n }
  }
}

// The classes above read their own backing field directly, so only the generated *setter* is ever
// the other end of the race they report: they cannot tell suppressing both accessors apart from
// suppressing the setter alone. In a @ThreadSafe class the accessors are reported in their own
// right, which is what pins the getter.
@ThreadSafe
class ThreadSafeAccessors {
  @VisibleForTesting var fieldAnnotatedOk: Long = 0L

  @PropertyTargetVisibleForTesting var syntheticAnnotatedOk: Long = 0L

  var notAnnotatedBad: Long = 0L
}

// A name is not enough to identify a generated accessor, so the matcher requires the signature
// one must have before it consults any annotation. Each method below is named exactly after the
// annotated property beside it -- that name match is the whole point, so the `Bad` suffix is on
// the property rather than the method -- and is production code that cannot be an accessor and
// must keep reporting. The unannotated `liveness` property is the control that the class is
// reported on at all. Found by review.

// The "is" route: isPrimeBad takes an argument, so it is not the getter of the property.
@ThreadSafe
class IsRouteArity {
  @VisibleForTesting var isPrimeBad: Boolean = false

  var liveness: Long = 0L

  fun isPrimeBad(n: Int): Boolean {
    liveness = n.toLong()
    return isPrimeBad
  }
}

// The "get" route: an overload the compiler never generated, beside the real synthetic
// getClockBad$annotations() that the annotated property does generate.
@ThreadSafe
class GetRouteArity {
  @PropertyTargetVisibleForTesting var clockBad: Long = 0L

  var liveness: Long = 0L

  fun getClockBad(which: Int): Long {
    liveness = which.toLong()
    return clockBad
  }
}

// The "set" route: setUpBad takes no argument, so it is not the setter of the property upBad,
// whose generated setter is setUpBad(Boolean).
@ThreadSafe
class SetRouteArity {
  @VisibleForTesting var upBad: Boolean = false

  var liveness: Long = 0L

  fun setUpBad() {
    liveness = 1L
  }
}

// With the @get: use-site target the annotation is on the getter itself, which RacerD already
// excused before this change; the setter stays a production write and must keep reporting.
@ThreadSafe
class GetOnlyAnnotation {
  @get:PropertyTargetVisibleForTesting var clockBad: Long = 0L

  var liveness: Long = 0L
}
