/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package codetoanalyze.java.infer;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PipedReader;
import java.io.PipedWriter;
import java.io.PushbackReader;
import java.io.Reader;

public class ReaderLeaks {

  private void ignore(Object o) {}

  // Reader  tests

  public void readerNotClosedAfterReadBad() {
    Reader r;
    try {
      r = new FileReader("testing.txt");
      r.read();
      r.close();
    } catch (IOException e) {
    }
  }

  public void readerClosedOk() throws IOException {
    Reader r = null;
    try {
      r = new FileReader("testing.txt");
      boolean ready = r.ready();
      r.close();
    } catch (IOException e) {
    } finally {
      if (r != null) r.close();
    }
  }

  // BufferedReader  tests

  public void bufferedReaderNotClosedAfterReadBad() {
    BufferedReader reader;
    try {
      reader = new BufferedReader(new FileReader("testing.txt"));
      ignore(reader.read());
      reader.close();
    } catch (IOException e) {
    }
  }

  public void bufferedReaderClosedOk() throws IOException {
    BufferedReader reader = null;
    try {
      reader = new BufferedReader(new FileReader("testing.txt"));
      ignore(reader.read());
    } catch (IOException e) {
    } finally {
      if (reader != null) reader.close();
    }
  }

  // Former FN: readLine was unknown, so this unclosed wrapper was not tracked.
  public void tryWithBufferReaderWrapperBad(File file) throws IOException {
    try (InputStreamReader inputStreamReader = new InputStreamReader(new FileInputStream(file))) {
      BufferedReader reader = new BufferedReader(inputStreamReader);
      ignore(reader.readLine());
    }
  }

  // readLine may throw IOException and does not close the reader.
  public void bufferedReaderNotClosedAfterReadLineBad() throws IOException {
    BufferedReader reader = new BufferedReader(new FileReader("testing.txt"));
    reader.readLine();
  }

  public void bufferedReaderClosedAfterReadLineOk() throws IOException {
    try (BufferedReader reader = new BufferedReader(new FileReader("testing.txt"))) {
      reader.readLine();
    }
  }

  public void tryWithBufferReaderWrapperOk(File file) throws IOException {
    try (InputStreamReader inputStreamReader = new InputStreamReader(new FileInputStream(file));
        BufferedReader reader = new BufferedReader(inputStreamReader); ) {
      ignore(reader.readLine());
    }
  }

  // InputStreamReader  tests

  public void inputStreamReaderNotClosedAfterReadBad() {
    InputStreamReader reader;
    try {
      reader = new InputStreamReader(new FileInputStream("testing.txt"));
      reader.read();
      reader.close();
    } catch (IOException e) {
    }
  }

  public void inputStreamReaderClosedOk() throws IOException {
    InputStreamReader reader = null;
    try {
      reader = new InputStreamReader(new FileInputStream("testing.txt"));
      ignore(reader.read());
    } catch (IOException e) {
    } finally {
      if (reader != null) reader.close();
    }
  }

  // FileReader  tests

  public void fileReaderNotClosedAfterReadBad() {
    FileReader reader;
    try {
      reader = new FileReader("testing.txt");
      reader.read();
      reader.close();
    } catch (IOException e) {
    }
  }

  public void fileReaderClosedOk() throws IOException {
    FileReader reader = null;
    try {
      reader = new FileReader("testing.txt");
      reader.read();
    } catch (IOException e) {
    } finally {
      if (reader != null) reader.close();
    }
  }

  // PushbackReader  tests

  public void pushbackReaderNotClosedAfterReadBad() {
    PushbackReader reader;
    try {
      reader = new PushbackReader(new InputStreamReader(new FileInputStream("testing.txt")));
      reader.read();
      reader.close();
    } catch (IOException e) {
    }
  }

  public void pushbackReaderClosedOk() throws IOException {
    PushbackReader reader = null;
    try {
      reader = new PushbackReader(new InputStreamReader(new FileInputStream("testing.txt")));
      reader.read();
    } catch (IOException e) {
    } finally {
      if (reader != null) reader.close();
    }
  }

  // PipedReader tests

  public void pipedReaderNotClosedAfterConstructedWithWriterBad() {
    PipedReader reader;
    PipedWriter writer;
    try {
      writer = new PipedWriter();
      reader = new PipedReader(writer);
      reader.read();
      reader.close();
    } catch (IOException e) {
    }
  }

  public void pipedReaderNotClosedAfterReadBad(PipedWriter writer) {
    PipedReader reader;
    try {
      reader = new PipedReader();
      reader.connect(writer);
      reader.read();
      reader.close();
    } catch (IOException e) {
    }
  }

  public void pipedReaderNotConnectedBad(PipedWriter writer) {
    PipedReader reader;
    try {
      reader = new PipedReader();
      reader.connect(writer);
      reader.close();
    } catch (IOException e) {
    }
  }

  public void pipedReaderClosedOk(PipedWriter writer) throws IOException {
    PipedReader reader = null;
    try {
      reader = new PipedReader();
      reader.connect(writer);
      reader.read();
    } catch (IOException e) {
    } finally {
      if (reader != null) reader.close();
    }
  }

  public void FN_pipedReaderWriterBad() throws IOException {
    PipedReader reader = null;
    PipedWriter writer = new PipedWriter();
    try {
      reader = new PipedReader(writer);
      reader.read();
    } catch (IOException e) {
    } finally {
      if (reader != null) reader.close();
    }
  }

  public void pipedReaderWriterOk() throws IOException {
    PipedReader reader = null;
    PipedWriter writer = new PipedWriter();
    try {
      reader = new PipedReader(writer);
      reader.read();
    } catch (IOException e) {
    } finally {
      if (reader != null) reader.close();
      else writer.close();
    }
  }

  // Two readers opened by one call, each closed on one branch only, so both leak. Each is
  // reported at its earliest way out, which is the branch that closes the other reader, so the
  // two entries expected below name different last accesses and the error log, which keys an
  // entry on its message, keeps them apart.
  //
  // Both readers also become unreachable together at the end of the method, and those two
  // reports would read the same -- same constructor, same call, same last access -- so one of
  // them would be folded into the other there. Neither entry below is reported from that point,
  // but what this test pins is that such a fold is not counted as a report already made: were it
  // counted, the reader it merged away would be suppressed everywhere and its leak would go
  // unreported.

  static class TwoReaders {
    BufferedReader first;
    BufferedReader second;

    TwoReaders(BufferedReader first, BufferedReader second) {
      this.first = first;
      this.second = second;
    }
  }

  static TwoReaders openTwoReaders() throws IOException {
    BufferedReader first = new BufferedReader(new FileReader("first.txt"));
    BufferedReader second = new BufferedReader(new FileReader("second.txt"));
    return new TwoReaders(first, second);
  }

  public void twoReadersFromOneCallBad(boolean b) throws IOException {
    TwoReaders readers = openTwoReaders();
    if (b) {
      readers.first.close();
    } else {
      readers.second.close();
    }
  }

  public void twoReadersFromOneCallOk() throws IOException {
    TwoReaders readers = openTwoReaders();
    try {
      readers.first.close();
    } finally {
      readers.second.close();
    }
  }

  // One reader that becomes unreachable on two ways out of the method. That is one leak and one
  // report, and the deduplication is right to collapse it; what it may never do is drop both.
  // Named apart from the reader tests above because this one pins the count, not just the leak.
  public void oneReaderNotClosedAfterReadLineBad() throws IOException {
    BufferedReader reader = new BufferedReader(new FileReader("testing.txt"));
    reader.readLine();
  }

  public void oneReaderClosedAfterReadLineOk() throws IOException {
    try (BufferedReader reader = new BufferedReader(new FileReader("testing.txt"))) {
      reader.readLine();
    }
  }
}
