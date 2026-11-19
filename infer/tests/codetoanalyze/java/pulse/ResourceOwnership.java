/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package codetoanalyze.java.infer;

import java.io.BufferedInputStream;
import java.io.ByteArrayOutputStream;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;
import java.util.zip.ZipOutputStream;

class ResourceOwnership {
  static class Worker extends Thread {
    final Socket socket;
    Worker(Socket socket) { this.socket = socket; }
    public void run() {
      try { socket.getInputStream().read(); }
      catch (IOException ignored) {}
      finally { try { socket.close(); } catch (IOException ignored) {} }
    }
  }
  static void acceptedThreadOk(ServerSocket server) throws IOException {
    Socket socket = server.accept();
    new Worker(socket).start();
  }
  static void acceptedLambdaThreadOk(ServerSocket server) throws IOException {
    Socket socket = server.accept();
    new Thread(() -> {
      try { socket.getInputStream().read(); }
      catch (IOException ignored) {}
      finally { try { socket.close(); } catch (IOException ignored) {} }
    }).start();
  }
  static void acceptedFinallyClosedOk(ServerSocket server) throws IOException {
    Socket socket = server.accept();
    try { socket.getInputStream().read(); }
    finally { socket.close(); }
  }
  static void acceptedAccessorExceptionBad(ServerSocket server) throws IOException {
    Socket socket = server.accept();
    socket.getInputStream().read();
    socket.close();
  }
  static void constructorAccessorExceptionBad() throws IOException {
    Socket socket = new Socket("localhost", 9876);
    socket.getInputStream().read();
    socket.close();
  }
  static void acceptedUnclosedBad(ServerSocket server) throws IOException {
    Socket socket = server.accept();
    socket.getInputStream();
  }
  static void callerWrapperFinishedOk(OutputStream caller) throws IOException {
    ZipOutputStream zip = new ZipOutputStream(caller);
    zip.putNextEntry(new ZipEntry("test"));
    zip.closeEntry();
    zip.finish();
  }
  static void ownedUnderlyingFinishedBad() throws IOException {
    ZipOutputStream zip = new ZipOutputStream(new FileOutputStream("test.zip"));
    zip.putNextEntry(new ZipEntry("test"));
    zip.closeEntry();
    zip.finish();
  }
  static void zipFinallyClosedOk(InputStream caller) throws IOException {
    ZipInputStream zip = new ZipInputStream(caller);
    try { while (zip.getNextEntry() != null) { zip.read(); } }
    finally { zip.close(); }
  }
  static void zipUnclosedBad(InputStream caller) throws IOException {
    ZipInputStream zip = new ZipInputStream(caller);
    zip.getNextEntry();
  }
  static void closeEveryPathOk(boolean choice) throws IOException {
    FileInputStream input = new FileInputStream("test");
    if (choice) { input.close(); return; }
    input.close();
  }
  static Runnable returnedCaptureOk(ServerSocket server) throws IOException {
    Socket socket = server.accept();
    return () -> { try { socket.close(); } catch (IOException ignored) {} };
  }
  static void unusedCaptureBad(ServerSocket server) throws IOException {
    Socket socket = server.accept();
    Runnable unused = () -> { try { socket.close(); } catch (IOException ignored) {} };
  }
  static void unstartedCaptureThreadBad(ServerSocket server) throws IOException {
    Socket socket = server.accept();
    Thread unused = new Thread(() -> { try { socket.close(); } catch (IOException ignored) {} });
  }
  static void unknownCaptureConsumerOk(ServerSocket server) throws IOException {
    Socket socket = server.accept();
    consumeCapture(() -> { try { socket.close(); } catch (IOException ignored) {} });
  }
  static native void consumeCapture(Runnable action);
  static void groupThreadOk(ServerSocket server, ThreadGroup group) throws IOException {
    Socket socket = server.accept();
    new Thread(group, () -> { try { socket.close(); } catch (IOException ignored) {} }, "worker", 0).start();
  }
  static void namedThreadOk(ServerSocket server) throws IOException {
    Socket socket = server.accept();
    new Thread(() -> { try { socket.close(); } catch (IOException ignored) {} }, "worker").start();
  }
  static void unstartedButClosedOk(ServerSocket server) throws IOException {
    Socket socket = server.accept();
    Thread unused = new Thread(() -> { try { socket.close(); } catch (IOException ignored) {} });
    socket.close();
  }
  static void nullThreadBad() { Thread thread = null; thread.start(); }
  static void borrowedZipHelper(OutputStream caller) throws IOException {
    ZipOutputStream zip = new ZipOutputStream(caller);
    zip.putNextEntry(new ZipEntry("test")); zip.closeEntry(); zip.finish();
  }
  static void ownedThroughBorrowedHelperBad() throws IOException {
    FileOutputStream output = new FileOutputStream("test.zip");
    borrowedZipHelper(output);
  }
  static void nestedZipFinallyClosedOk(InputStream input) throws IOException {
    ZipInputStream zip = new ZipInputStream(new BufferedInputStream(input));
    try { while (zip.getNextEntry() != null) { zip.read(); } }
    finally { zip.close(); }
  }
  static class NotStarted extends Thread {
    NotStarted(Runnable action) { super(action); }
    @Override public void start() { }
  }
  static void overriddenStartBad(ServerSocket server) throws IOException {
    Socket socket = server.accept();
    new NotStarted(() -> { try { socket.close(); } catch (IOException ignored) { } }).start();
  }
  static class IgnoresAction extends Thread {
    IgnoresAction(Runnable action) { super(); }
  }
  static void ignoredActionBad(ServerSocket server) throws IOException {
    Socket socket = server.accept();
    new IgnoresAction(() -> { try { socket.close(); } catch (IOException ignored) { } }).start();
  }
  static void nullGroupThreadOk(ServerSocket server) throws IOException {
    Socket socket = server.accept();
    new Thread(null, () -> { try { socket.close(); } catch (IOException ignored) { } }).start();
  }
  static void unstartedNullGroupThreadBad(ServerSocket server) throws IOException {
    Socket socket = server.accept();
    new Thread(null, () -> { try { socket.close(); } catch (IOException ignored) { } });
  }
  static void unstartedNamedThreadBad(ServerSocket server) throws IOException {
    Socket socket = server.accept();
    Thread thread = new Thread(() -> { try { socket.close(); } catch (IOException ignored) { } });
    thread.getName();
  }
  static void freshMemoryZipUnclosedBad() throws IOException {
    ZipOutputStream zip = new ZipOutputStream(new ByteArrayOutputStream());
    zip.putNextEntry(new ZipEntry("test"));
  }
}
