/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package kotlinx.coroutines;

import com.facebook.infer.annotation.ThreadSafe;

// Minimal declarations of the thread-safe library interfaces.
interface Job {
  boolean isActive();
  void join();
  void cancel();
}

interface CompletableJob extends Job {
  boolean complete();
}

interface Deferred extends Job {
  Object await(Object continuation);
}

interface CompletableDeferred extends Deferred {
  boolean complete(Object value);
}

interface UnannotatedTask {
  Object await(Object continuation);
}

// A user extension's additional operation has no library thread-safety guarantee.
interface ExtendedDeferred extends Deferred {
  void updateUserState();
}

@ThreadSafe
class CoroutineInterfaces {
  private int state;

  boolean jobActive_ok(Job job) { return job.isActive(); }
  void jobJoin_ok(Job job) { job.join(); }
  void jobCancel_ok(Job job) { job.cancel(); }
  boolean jobComplete_ok(CompletableJob job) { return job.complete(); }
  Object deferredAwait_ok(Deferred deferred) { return deferred.await(null); }
  Object completableAwait_ok(CompletableDeferred deferred) { return deferred.await(null); }
  boolean deferredComplete_ok(CompletableDeferred deferred) { return deferred.complete(new Object()); }

  Object unrelatedInterface_bad(UnannotatedTask task) { return task.await(null); }
  void userExtension_bad(ExtendedDeferred deferred) { deferred.updateUserState(); }
  void writeCallerState_bad(CompletableDeferred deferred) {
    deferred.complete(new Object());
    state++;
  }
}
