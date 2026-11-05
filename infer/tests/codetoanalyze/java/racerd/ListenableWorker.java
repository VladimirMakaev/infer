/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Minimal stand-in for androidx.work.ListenableWorker, which is not on the test classpath.
package androidx.work;

public abstract class ListenableWorker {
  public abstract int startWork();
}
