/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package codetoanalyze.java.checkers;

import com.facebook.infer.annotation.ThreadSafe;
import com.google.common.annotations.VisibleForTesting;

// A Kotlin property's @VisibleForTesting lands on the backing field, and RacerD reads it there to
// excuse the property's generated accessors. In Java the annotation says the field itself is
// visible for testing and says nothing about a getter or setter that shares its name, so these
// two are production methods and keep reporting.
@ThreadSafe
class VisibleForTestingField {
  @VisibleForTesting int clockBad;

  public int getClockBad() {
    return clockBad;
  }

  public void setClockBad(int clock) {
    clockBad = clock;
  }
}
