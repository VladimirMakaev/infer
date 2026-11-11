/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package codetoanalyze.java.checkers;

import javax.annotation.concurrent.ThreadSafe;

@ThreadSafe
public class CallsiteFields {
  private int first;
  private int second;
  private long ticks;
  private Object value;

  public void grouped_bad(int seed, Object nextValue) {
    reset(seed, nextValue);
  }

  public void other_grouped_bad(int seed, Object nextValue) {
    reset(seed, nextValue);
  }

  public synchronized void grouped_locked_ok(int seed, Object nextValue) {
    reset(seed, nextValue);
  }

  // A separate access remains a true-positive control.
  public Object read_bad() {
    return value;
  }

  // Different fields at different call sites must not be collapsed together.
  public void distinct_sites_bad() {
    writeFirst();
    writeSecond();
  }

  // Only the unprotected field belongs in this call site's report.
  public void mixed_bad() {
    mixed();
  }

  private void reset(int seed, Object nextValue) {
    first = seed;
    second = seed;
    ticks = seed;
    value = nextValue;
  }

  private void writeFirst() {
    first = 0;
  }

  private void writeSecond() {
    second = 0;
  }

  private void mixed() {
    first = 0;
    synchronized (this) {
      value = null;
    }
  }
}
