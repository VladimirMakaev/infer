/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Minimal stand-in for the platform StateMachine State: it is in com.android.internal and so is
// not on any public classpath. Only the shape the confinement contract matches is needed.
package com.android.internal.util;

import android.os.Message;

public abstract class State {
  public void enter() {}

  public void exit() {}

  public boolean processMessage(Message msg) {
    return false;
  }
}
