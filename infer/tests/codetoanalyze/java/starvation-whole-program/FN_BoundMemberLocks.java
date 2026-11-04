/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

class FN_BoundMemberLocksTask implements Runnable {
  final Object mLock = new Object();
  FN_BoundMemberLocksOtherTask mOther;

  // Known FN; see the parent/child ordering follow-up.
  @Override
  public void run() {
    synchronized (mLock) {
      synchronized (mOther.mLock) {
      }
    }
  }
}

class FN_BoundMemberLocksOtherTask implements Runnable {
  final Object mLock = new Object();
  FN_BoundMemberLocksTask mTask;

  // Known FN; see the parent/child ordering follow-up.
  @Override
  public void run() {
    synchronized (mLock) {
      synchronized (mTask.mLock) {
      }
    }
  }
}

class FN_BoundMemberLocksMain {
  // Known FN; see the parent/child ordering follow-up (explicit cyclic task back-pointers).
  public static void main(String args[]) {
    FN_BoundMemberLocksTask task = new FN_BoundMemberLocksTask();
    FN_BoundMemberLocksOtherTask other = new FN_BoundMemberLocksOtherTask();
    task.mOther = other;
    other.mTask = task;
    Thread t1 = new Thread(task);
    t1.start();
    Thread t2 = new Thread(other);
    t2.start();
  }
}
