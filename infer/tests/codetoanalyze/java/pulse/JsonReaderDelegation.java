/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
import android.util.JsonReader;
import java.io.*;

class JsonReaderDelegation {
  static void fileJsonReaderClosedOk(String path) throws IOException {
    FileInputStream file = new FileInputStream(path);
    InputStreamReader input = new InputStreamReader(file);
    JsonReader reader = new JsonReader(input);
    reader.close();
  }

  static void fileJsonReaderNotClosedBad(String path) throws IOException {
    FileInputStream file = new FileInputStream(path);
    InputStreamReader input = new InputStreamReader(file);
    JsonReader reader = new JsonReader(input);
  }

  static void memoryJsonReaderClosedOk() throws IOException {
    StringReader input = new StringReader("{}");
    JsonReader reader = new JsonReader(input);
    reader.close();
  }

  static void borrowedReaderClosedOk(Reader input) throws IOException {
    JsonReader reader = new JsonReader(input);
    reader.close();
  }

  static void memoryJsonReaderNotClosedBad() {
    JsonReader reader = new JsonReader(new StringReader("{}"));
  }

  static void caughtNullConstructorPathBad() {
    try {
      new JsonReader(null);
    } catch (NullPointerException expected) {
      Object invalid = null;
      invalid.toString();
    }
  }

  static void caughtNullConstructorOk() {
    try {
      new JsonReader(null);
    } catch (NullPointerException expected) {}
  }
}
