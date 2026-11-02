/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package codetoanalyze.java.classpathwithoutplatform;

import java.io.BufferedWriter;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.io.Writer;

// Captured from compiled output, with a classpath that holds no platform classes: the type
// environment then has java.io.Writer and friends as placeholders with no supertypes, and the
// models keyed on those supertypes stop matching.
public class Streams {

  // The writer chain owns the stream, and closing the outermost writer closes all of it.
  public static void wrappedWriterClosedOk(String name) throws IOException {
    Writer writer = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(name)));
    try {
      writer.write("x");
    } finally {
      writer.close();
    }
  }

  // Nothing closes the chain, so the stream it owns leaks too.
  public static void wrappedWriterNeverClosedBad(String name) throws IOException {
    Writer writer = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(name)));
    writer.write("x");
  }

  // No wrapper, closed.
  public static void streamClosedOk(String name) throws IOException {
    FileOutputStream stream = new FileOutputStream(name);
    try {
      stream.write(1);
    } finally {
      stream.close();
    }
  }

  // No wrapper, never closed: the control that reports whatever the type environment holds.
  public static void streamNeverClosedBad(String name) throws IOException {
    FileOutputStream stream = new FileOutputStream(name);
    stream.write(1);
  }
}
