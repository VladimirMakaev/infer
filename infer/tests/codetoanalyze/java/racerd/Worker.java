/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Minimal stand-in for androidx.work.Worker, which is not on the test classpath. WorkManager
// builds a fresh instance per run and calls doWork once on it.
package androidx.work;

public abstract class Worker extends ListenableWorker {
  public abstract int doWork();

  @Override
  public int startWork() {
    return doWork();
  }
}
