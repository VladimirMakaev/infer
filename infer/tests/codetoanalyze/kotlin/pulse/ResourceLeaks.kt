/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
package codetoanalyze.kotlin.pulse

import java.io.BufferedReader
import java.io.File
import java.io.FileInputStream
import java.io.InputStreamReader

class ResourceLeaks {

  // `use` closes the reader, and closing the reader closes the stream under it.
  fun useOnWrappedReaderOk(path: String): String? = File(path).bufferedReader().use { it.readLine() }

  // `use` on a stream that wraps nothing.
  fun useOnStreamOk(path: String): Int = File(path).inputStream().use { it.read() }

  // The wrapper chain is closed by hand through its outermost reader.
  fun closeWrappedReaderOk(file: File) {
    val reader = BufferedReader(InputStreamReader(FileInputStream(file)))
    reader.close()
  }

  // One level of wrapping, closed.
  fun closeReaderOk(file: File) {
    val reader = InputStreamReader(FileInputStream(file))
    reader.close()
  }

  // No wrapper, closed.
  fun closeStreamOk(file: File) {
    val stream = FileInputStream(file)
    stream.close()
  }

  // Nothing closes the wrapper, so the stream it owns leaks too.
  fun wrappedReaderNeverClosedBad(file: File): String? {
    val reader = BufferedReader(InputStreamReader(FileInputStream(file)))
    return reader.readLine()
  }

  // Nothing closes the stream.
  fun streamNeverClosedBad(file: File): Int {
    val stream = FileInputStream(file)
    return stream.read()
  }
}
