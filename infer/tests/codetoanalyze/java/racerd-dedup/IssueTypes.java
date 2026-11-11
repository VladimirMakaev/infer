/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package codetoanalyze.java.checkers;

import javax.annotation.concurrent.GuardedBy;
import javax.annotation.concurrent.ThreadSafe;

@ThreadSafe
public class IssueTypes {
  @GuardedBy("this") private int guarded;
  private int plain;

  // The call has a GuardedBy violation and a thread-safety violation.
  public void two_types_bad(int value) {
    writeBoth(value);
  }

  public synchronized void two_types_locked_ok(int value) {
    writeBoth(value);
  }

  public int read_plain_bad() {
    return plain;
  }

  private void writeBoth(int value) {
    guarded = value;
    plain = value;
  }
}
