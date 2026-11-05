/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package codetoanalyze.java.checkers;

import android.os.Handler;
import android.os.Message;
import androidx.work.ListenableWorker;
import androidx.work.Worker;
import com.android.internal.util.State;
import com.google.android.exoplayer2.Renderer;

/**
 * A framework callback that the framework itself runs on one thread cannot race with ITSELF.
 *
 * <p>Without a contract for it, any lock such a callback takes promotes its own later accesses to
 * AnyThread -- update_for_lock_use fires on release too -- and the callback is then reported
 * against itself on a field no second thread ever touches. The _ok cases below are exactly that
 * shape and must be silent. The _bad cases pin what the contracts must NOT silence.
 */
class ConfinementContracts {}

// ----------------------------------------------------------------------------
// Handler: bound to one Looper at construction, dispatches to it serially.
// ----------------------------------------------------------------------------
class HandlerConfined extends Handler {
  private final Object lock = new Object();
  private int guarded;
  private int state;

  @Override
  public void handleMessage(Message msg) {
    synchronized (lock) {
      guarded++;
    }
    // Lock use above promoted this method to AnyThread before the contract existed.
    state++;
  }
}

// A confined callback still races with an UNCONFINED public sibling: the contract says the
// callback does not race with itself, not that the field is private to one thread.
class HandlerConfinedWithPublicSibling extends Handler {
  private final Object lock = new Object();
  private int guarded;
  private int state;

  @Override
  public void handleMessage(Message msg) {
    synchronized (lock) {
      guarded++;
    }
    state++;
  }

  // Any thread may call this.
  public void publicApiBad() {
    synchronized (lock) {
      guarded++;
    }
    state++;
  }
}

// ----------------------------------------------------------------------------
// StateMachine: every State callback runs on the single SmHandler thread.
// ----------------------------------------------------------------------------
class StateConfined extends State {
  private final Object lock = new Object();
  private int guarded;
  private int state;

  @Override
  public void enter() {
    synchronized (lock) {
      guarded++;
    }
    state++;
  }

  @Override
  public boolean processMessage(Message msg) {
    synchronized (lock) {
      guarded++;
    }
    return state++ > 0;
  }
}

// ----------------------------------------------------------------------------
// WorkManager: a fresh Worker per run, doWork called once on that instance.
// ----------------------------------------------------------------------------
class WorkerConfined extends Worker {
  private final Object lock = new Object();
  private int guarded;
  private int result;

  @Override
  public int doWork() {
    synchronized (lock) {
      guarded++;
    }
    result++;
    return result;
  }
}

class ListenableWorkerConfined extends ListenableWorker {
  private final Object lock = new Object();
  private int guarded;
  private int result;

  @Override
  public int startWork() {
    synchronized (lock) {
      guarded++;
    }
    result++;
    return result;
  }
}

// ----------------------------------------------------------------------------
// ExoPlayer: a Renderer is driven only from the playback thread.
// ----------------------------------------------------------------------------
class RendererConfined implements Renderer {
  private final Object lock = new Object();
  private int guarded;
  private int droppedBufferCount;

  @Override
  public void render() {
    synchronized (lock) {
      guarded++;
    }
    droppedBufferCount++;
  }

  @Override
  public void onPositionReset() {
    synchronized (lock) {
      guarded++;
    }
    droppedBufferCount = 0;
  }
}

// ----------------------------------------------------------------------------
// Controls that must KEEP reporting. These are the counterexample and siblings from
// decision_racerd_lock_use; if a contract ever silences one of these it is too broad.
// ----------------------------------------------------------------------------

// A: the never-locked field. `unguarded` is never accessed under a lock, so the rejected
// mixed-discipline filter would have deleted this; the confinement route must not.
class NeverLockedFieldNotConfined {
  private final Object lock = new Object();
  private int guarded;
  private int unguarded;

  public void writeUnguardedBad() {
    synchronized (lock) {
      guarded++;
    }
    unguarded++;
  }

  public int readUnguardedBad() {
    synchronized (lock) {
      guarded++;
    }
    return unguarded;
  }

  public int guardedOk() {
    synchronized (lock) {
      return guarded;
    }
  }
}

// An ordinary method named like a callback but on no confined type: still reports.
class LooksLikeACallbackButIsNotBad {
  private final Object lock = new Object();
  private int guarded;
  private int state;

  public void handleMessage(Message msg) {
    synchronized (lock) {
      guarded++;
    }
    state++;
  }
}

// A non-contract method on a confined CLASS is not itself confined by these contracts.
class HandlerOtherMethodBad extends Handler {
  private final Object lock = new Object();
  private int guarded;
  private int state;

  public void notACallbackBad() {
    synchronized (lock) {
      guarded++;
    }
    state++;
  }
}
