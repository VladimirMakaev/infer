/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
package android.util;

import java.io.Closeable;
import java.io.IOException;
import java.io.Reader;

// API-shaped capture boundary: the platform close implementation is not captured.
public final class JsonReader implements Closeable {
  private final Reader in;
  public JsonReader(Reader in) {
    if (in == null) {
      throw new NullPointerException("in == null");
    }
    this.in = in;
  }
  public native void close() throws IOException;
}
