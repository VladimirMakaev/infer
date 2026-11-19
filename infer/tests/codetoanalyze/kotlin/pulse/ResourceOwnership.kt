/*
* Copyright (c) Facebook, Inc. and its affiliates.
*
* This source code is licensed under the MIT license found in the
* LICENSE file in the root directory of this source tree.
*/
package codetoanalyze.kotlin.pulse

import java.io.*
import java.net.*
import java.util.zip.*

class ResourceOwnership {
  fun acceptedUseOk(server: ServerSocket) {
    server.accept().use { it.getInputStream().read() }
  }
  fun acceptedThreadUseOk(server: ServerSocket) {
    val socket = server.accept()
    Thread { socket.use { it.getInputStream().read() } }.start()
  }
  fun zipUseOk(input: InputStream) {
    ZipInputStream(BufferedInputStream(input)).use { zip ->
      while (zip.nextEntry != null) { zip.read() }
    }
  }
  fun ownedZipUseOk() {
    ZipInputStream(BufferedInputStream(FileInputStream("test.zip"))).use { zip ->
      while (zip.nextEntry != null) { zip.read() }
    }
  }
  fun callerZipApplyFinishedOk(output: OutputStream) {
    ZipOutputStream(output).apply {
      putNextEntry(ZipEntry("test")); closeEntry(); finish()
    }
  }
  fun getterExceptionBad(server: ServerSocket) {
    val socket = server.accept()
    socket.getInputStream().read()
    socket.close()
  }
  fun ownedGetterExceptionBad() {
    val socket = Socket("localhost", 12345)
    socket.getInputStream().read()
    socket.close()
  }
  fun unusedThreadBad(server: ServerSocket) {
    val socket = server.accept()
    Thread { socket.use { } }
  }
  fun useBodyLeakBad(server: ServerSocket) {
    server.accept().use { FileInputStream("test").read() }
  }
  fun useBodyExceptionOk(server: ServerSocket) {
    server.accept().use { throw IOException("test") }
  }
  fun zipSingleUseOk(input: InputStream) { ZipInputStream(input).use { } }
  fun bufferedUseOk(input: InputStream) { BufferedInputStream(input).use { } }
  fun nestedZipEmptyUseOk(input: InputStream) { ZipInputStream(BufferedInputStream(input)).use { } }
  fun namedNestedZipEmptyUseOk(input: InputStream) {
    val buffer = BufferedInputStream(input)
    val zip = ZipInputStream(buffer)
    zip.use { }
  }
  // Missing without the stdlib finalizer body in the capture; keep the limitation visible.
  fun FN_finalizerExceptionBad(socket: Socket) {
    val other = FileInputStream("test")
    socket.use { }
    other.close()
  }
  fun nullableUseOk() {
    val other = FileInputStream("test")
    val socket: Socket? = null
    socket.use { }
    other.close()
  }
  fun caughtFinalizerExceptionOk(socket: Socket) {
    val other = FileInputStream("test")
    try { socket.use { } } catch (ignored: IOException) { }
    other.close()
  }
}
