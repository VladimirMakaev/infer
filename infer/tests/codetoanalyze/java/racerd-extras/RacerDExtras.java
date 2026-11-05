/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package codetoanalyze.java.checkers;

import java.util.Map;
import javax.annotation.concurrent.GuardedBy;
import javax.annotation.concurrent.ThreadSafe;

/*
 * Every race below is an ordinary one that the other racerd suites already cover. What this suite
 * pins is the structured description of the access that each report carries alongside the message:
 * the Makefile adds racerd_access_extra to the printed fields, so issues.exp here records the
 * declaring class, field name, access path, kind, lock and thread of the access. A consumer that
 * groups reports by field reads those, and the qualifier text is not a stable interface for it.
 */

/** (1) a plain instance field: field_class is the class that declares it, path is this.count */
@ThreadSafe
class Counter {
  private int count;

  public synchronized void incrementOk() {
    count++;
  }

  public int readCountBad() {
    return count;
  }
}

/** (2) a static field declared in another class and raced on here, so this report is filed against
 * Caller.java while the field belongs to Settings */
@ThreadSafe
class Caller {
  public Object readSettingsBad() {
    return Settings.instance;
  }

  public synchronized void initSettingsOk() {
    Settings.instance = new Object();
  }
}

/** (3) a field reached through another object: the accessed field is the outermost one, state,
 * declared in Mgr -- not mgr, declared in Holder */
class Mgr {
  int state;
}

@ThreadSafe
class Holder {
  Mgr mgr;

  public int readStateBad() {
    return mgr.state;
  }

  public synchronized void setStateOk(int v) {
    mgr.state = v;
  }
}

/** (4) a container write, which names the container field and a kind of container_write */
@ThreadSafe
class Registry {
  Map<String, String> entries;

  public void putBad(String key, String value) {
    entries.put(key, value);
  }
}

/** (5) one field, two reports that differ only in whether the lock was held: the synchronized read
 * carries lock_held true, the unsynchronized write that it races carries lock_held false */
@ThreadSafe
class LockedRead {
  private int value;

  public synchronized int readLockedBad() {
    return value;
  }

  public void writeUnlockedBad(int v) {
    value = v;
  }
}

/*
 * Every access above runs on an unknown thread, so all of them carry thread: any, and a column
 * that is constant would be satisfied by a bug that mapped every domain value to "any". The
 * GuardedBy report below is reached without any evidence of a concurrent context, so it carries
 * thread: no_thread. It also pins the second of the two report variants that share this code
 * path: a GUARDEDBY_VIOLATION carries the entry just as a THREAD_SAFETY_VIOLATION does.
 */
class Guarded {
  private Object lock = new Object();

  @GuardedBy("lock")
  private int guardedValue;

  public void lockedWriteOk() {
    synchronized (lock) {
      guardedValue = 0;
    }
  }

  @GuardedBy("lock")
  private int unguardedValue;

  public void unlockedWriteBad() {
    unguardedValue = 1;
  }
}
