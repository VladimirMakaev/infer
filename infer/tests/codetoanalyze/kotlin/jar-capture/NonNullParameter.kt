/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
package codetoanalyze.kotlin.jarcapture

open class ControlBase

/**
 * The controls for [DefaultArguments]: a null really reaching a parameter that is declared
 * non-null must still be reported, in the same inner-class and default-arguments shapes. Without
 * these, lining the annotations up differently could silence the requirement altogether and the
 * fixture above would not notice.
 */
class NonNullParameter(val tag: String) {

  inner class Event(
      val mustNotBeNull: String,
      val optional: String? = null,
  ) : ControlBase()

  fun passNullToNonNullBad(): Any {
    val s: String? = null
    return Event(s!!, "x")
  }

  /** Through the default-arguments constructor specifically, the one the fix touches. */
  fun passNullToNonNullDefaultedBad(): Any {
    val s: String? = null
    return Event(s!!)
  }

  fun passRealStringOk(): Any = Event("fine")
}

class TopLevelNonNull(
    val a: String,
    val b: String? = null,
)

fun topLevelPassNullBad(): TopLevelNonNull {
  val s: String? = null
  return TopLevelNonNull(s!!)
}
