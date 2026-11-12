/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
import android.util.JsonReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.Socket;

class SocketAccessorExceptions {
  static void inputAccessorCloseOnlyNormalBad() throws IOException {
    Socket socket = new Socket();
    JsonReader reader = new JsonReader(new InputStreamReader(socket.getInputStream()));
    reader.close();
  }

  static void outputAccessorCloseOnlyNormalBad() throws IOException {
    Socket socket = new Socket();
    OutputStream stream = socket.getOutputStream();
    stream.close();
  }

  static void inputAccessorClosedFinallyOk() throws IOException {
    Socket socket = new Socket();
    try {
      JsonReader reader = new JsonReader(new InputStreamReader(socket.getInputStream()));
      reader.close();
    } finally {
      socket.close();
    }
  }

  static void outputAccessorClosedFinallyOk() throws IOException {
    Socket socket = new Socket();
    try {
      socket.getOutputStream().close();
    } finally {
      socket.close();
    }
  }

  static void throwingWriteThenChainBad() throws IOException {
    Socket socket = new Socket();
    socket.getOutputStream().write(1);
    JsonReader reader = new JsonReader(new InputStreamReader(socket.getInputStream()));
    reader.close();
  }

  static void inputAccessorOnBorrowedSocketOk(Socket socket) throws IOException {
    JsonReader reader = new JsonReader(new InputStreamReader(socket.getInputStream()));
    reader.close();
  }
}
