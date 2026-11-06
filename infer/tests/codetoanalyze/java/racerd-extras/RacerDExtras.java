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

/*
 * (6) a report that is not about a field at all. An INTERFACE_NOT_THREAD_SAFE warns about a call,
 * not an access to a named location, so there is no field to describe and the report carries NO
 * racerd_access entry: the issues.exp row below ends at its bug_trace, with nothing after it. That
 * absence is the contract. The alternative a consumer has to be protected from is an entry filled
 * in with the receiver -- here the formal `i`, whose declaring class is the interface -- which
 * would make a view that groups reports by field invent a field nobody accessed.
 */
interface ExtrasUnannotatedInterface {
  void run();
}

@ThreadSafe
class InterfaceCaller {
  public void callUnannotatedBad(ExtrasUnannotatedInterface i) {
    i.run();
  }
}

/*
 * (7) the third and last thread value, any_but_self, which no other test in the tree produces in a
 * reported access. It needs a read, not a write: an access known to be confined to the main thread
 * is not "unprotected" (AccessSnapshot.is_unprotected is false for any_but_self), and every write
 * report requires an unprotected write, so a main-thread write is simply silent. A main-thread
 * READ instead takes the protected-read path, which reports it against an unprotected write that
 * may run on any thread -- exactly the race below, where the writer is unsynchronized.
 *
 * Both methods report. The read is the row this case exists for, carrying thread: any_but_self;
 * the write's own row carries thread: any and keeps the two visibly distinct, so a regression that
 * collapsed the domain to a single value could not leave this pair unchanged. Deleting the
 * assertMainThread() call turns the read's value into any and changes nothing else, which is what
 * makes the column, rather than the shape of the race, the thing this case measures.
 */
class ExtrasThreadUtils {
  static void assertMainThread() {}
}

@ThreadSafe
class MainThreadConfined {
  private int value;

  public int readOnMainThreadBad() {
    ExtrasThreadUtils.assertMainThread();
    return value;
  }

  public void writeAnywhereBad(int v) {
    value = v;
  }
}
