/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
package codetoanalyze.kotlin.pulse

class MapGetTarget(val value: String)

// The null a map lookup can produce is the collection model's own assumption that the key is
// absent, not a value the program wrote, so `!!` is believed against it. A null the program
// really did produce is not, which is what keeps the Java-to-Kotlin cases in Kotlin.kt and
// BasicJavaKotlinBoundary.java reporting: `x!!`, `x as T` and an implicit platform-type
// coercion all compile to the same Intrinsics.checkNotNull, so the call itself cannot tell
// them apart and only the origin of the null can.
class MapGetAssertion {

  fun assertOnMapGetOk(m: Map<String, String>, k: String): MapGetTarget = MapGetTarget(m[k]!!)

  fun derefMapGetOk(m: Map<String, String>, k: String): Int = m[k]!!.length

  // a map of boxed primitives, so the assertion is followed by an unboxing
  fun unboxMapGetOk(m: Map<String, Int>, k: String): Int = m[k]!! + 1

  // a map filled by the program and then read on keys it registered
  private val registered = mutableMapOf<String, Int>()

  fun register(name: String, id: Int) {
    registered.put(name, id)
  }

  fun readRegisteredOk(): Int = registered.get("known")!! + 1

  // `as T` on a nullable compiles to the same intrinsic and is believed the same way
  fun castMapGetOk(m: Map<String, String>, k: String): MapGetTarget = MapGetTarget(m[k] as String)

  // A null the program produced is still reported, through the same `!!`.
  fun assertOnNullLocalBad(): Int {
    val s: String? = null
    return s!!.length
  }

  fun passNullLocalToNotNullParameterBad(): MapGetTarget {
    val s: String? = null
    return MapGetTarget(s!!)
  }
}
