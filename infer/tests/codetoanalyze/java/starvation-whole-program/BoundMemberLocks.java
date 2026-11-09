/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Two tasks whose locks are reached only through each other: the cycle between them is
// what makes the two orders meet on one pair of objects.
class BoundMemberLocksTask implements Runnable {
  final Object mLock = new Object();
  BoundMemberLocksOtherTask mOther;

  @Override
  public void run() {
    synchronized (mLock) {
      synchronized (mOther.mLock) {
      }
    }
  }
}

class BoundMemberLocksOtherTask implements Runnable {
  final Object mLock = new Object();
  BoundMemberLocksTask mTask;

  @Override
  public void run() {
    synchronized (mLock) {
      synchronized (mTask.mLock) {
      }
    }
  }
}

class BoundMemberLocksMain {
  public static void main(String args[]) {
    BoundMemberLocksTask task = new BoundMemberLocksTask();
    BoundMemberLocksOtherTask other = new BoundMemberLocksOtherTask();
    task.mOther = other;
    other.mTask = task;
    Thread t1 = new Thread(task);
    t1.start();
    Thread t2 = new Thread(other);
    t2.start();
  }
}
