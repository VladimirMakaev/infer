/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
package codetoanalyze.kotlin.jarcapture

enum class StopOrigin {
  UNKNOWN,
  USER,
}

open class BaseEvent

/**
 * An inner class has its outer instance ahead of the parameters it declares, but the parameter
 * annotations are indexed against the declared ones alone. The constructor the compiler
 * generates for the default arguments passes null for a defaulted nullable parameter, so if the
 * annotations are lined up from the front that null meets a non-null requirement written for a
 * different parameter.
 */
class DefaultArguments(val tag: String) {

  inner class UserEndedCasting(
      val sessionId: String? = null,
      val stopOrigin: StopOrigin = StopOrigin.UNKNOWN,
  ) : BaseEvent()

  inner class ErrorOccurred(
      val errorMessage: String,
      val sessionId: String? = null,
      val isProductServerDefinedReason: Boolean? = null,
  ) : BaseEvent()

  fun allDefaultsOk(): StopOrigin = UserEndedCasting().stopOrigin

  fun someDefaultsOk(): StopOrigin = UserEndedCasting("s").stopOrigin

  fun explicitNullOk(): String = ErrorOccurred("boom", null, null).errorMessage

  fun defaultedAfterNonNullOk(): String = ErrorOccurred("boom").errorMessage

  fun readDefaultedNullableOk(): String? = UserEndedCasting().sessionId
}

/** No outer instance, so the two lists already agree and nothing should change here. */
class TopLevelDefaults(
    val a: String,
    val b: String? = null,
) {
  fun readOk(): String = a
}
