/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package codetoanalyze.java.checkers;

import javax.annotation.concurrent.ThreadSafe;

/**
 * The exemption for a Kotlin coroutine's state machine is keyed on the field name AND on the class
 * being one of the continuation classes kotlinc generates. This class is not one: it extends
 * nothing, and it declares fields with exactly the names the state machine uses. Every field here
 * is written without the lock and read under it, so every one is a genuine race and must still be
 * reported. If the exemption ever matched on the field name alone, all of these would go silent.
 */
@ThreadSafe
class ContinuationScope {

  private final Object scopeLock = new Object();

  int label;
  Object result;
  Object L$0;
  int I$0;
  long J$0;

  void writeLabelBad() {
    label = 1;
  }

  int readLabelUnderLock() {
    synchronized (scopeLock) {
      return label;
    }
  }

  void writeResultBad() {
    result = new Object();
  }

  Object readResultUnderLock() {
    synchronized (scopeLock) {
      return result;
    }
  }

  void writeReferenceSlotBad() {
    L$0 = new Object();
  }

  Object readReferenceSlotUnderLock() {
    synchronized (scopeLock) {
      return L$0;
    }
  }

  void writeIntSlotBad() {
    I$0 = 2;
  }

  int readIntSlotUnderLock() {
    synchronized (scopeLock) {
      return I$0;
    }
  }

  void writeLongSlotBad() {
    J$0 = 3L;
  }

  long readLongSlotUnderLock() {
    synchronized (scopeLock) {
      return J$0;
    }
  }
}
