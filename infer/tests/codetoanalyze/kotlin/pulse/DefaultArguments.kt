/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
package codetoanalyze.kotlin.pulse

enum class StopOrigin {
  UNKNOWN,
  USER,
}

open class BaseEvent

/**
 * An inner class has an outer instance ahead of its declared parameters in the descriptor, but
 * the parameter annotations are indexed against the declared ones only. The synthetic
 * default-arguments constructor passes null for a defaulted nullable parameter, so if the
 * annotations land on the wrong parameter that null meets a @NotNull that belongs to its
 * neighbour.
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
