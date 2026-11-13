/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package codetoanalyze.java.infer;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

public class LeakAcrossSpecializations {

  interface Sink {
    void accept(String s);
  }

  static class ListSink implements Sink {
    List<String> out = new ArrayList<>();

    public void accept(String s) {
      out.add(s);
    }
  }

  static class CountSink implements Sink {
    int n;

    public void accept(String s) {
      n++;
    }
  }

  // One reader left behind on several ways out, in a procedure that is analysed more than once.
  //
  // The two call sites below fix a different dynamic type for `sink`, so this procedure is
  // analysed once per specialization, and each of those analyses starts with its own memory of
  // what it has already reported. Each settles on the earliest way out it can see, which is not
  // the same one, so the error log -- which keys an entry on its message -- receives several
  // entries it cannot tell are one finding. They are collapsed when the report is written, by
  // the allocation they are about, and the earliest of them is the one kept.
  static void drainBad(String path, Sink sink) throws IOException {
    BufferedReader reader = new BufferedReader(new FileReader(path));
    String first = reader.readLine();
    if (first == null) {
      return;
    }
    sink.accept(first);
    String line = reader.readLine();
    while (line != null) {
      sink.accept(line);
      line = reader.readLine();
    }
  }

  public void callerWithListSinkBad(String path) throws IOException {
    drainBad(path, new ListSink());
  }

  public void callerWithCountSinkBad(String path) throws IOException {
    drainBad(path, new CountSink());
  }

  static void drainOk(String path, Sink sink) throws IOException {
    BufferedReader reader = new BufferedReader(new FileReader(path));
    try {
      String line = reader.readLine();
      while (line != null) {
        sink.accept(line);
        line = reader.readLine();
      }
    } finally {
      reader.close();
    }
  }

  public void callerWithListSinkOk(String path) throws IOException {
    drainOk(path, new ListSink());
  }

  public void callerWithCountSinkOk(String path) throws IOException {
    drainOk(path, new CountSink());
  }
}
