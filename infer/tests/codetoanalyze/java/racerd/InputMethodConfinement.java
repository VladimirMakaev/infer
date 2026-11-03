/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package codetoanalyze.java.checkers;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.view.View;
import android.view.inputmethod.BaseInputConnection;
import javax.annotation.concurrent.ThreadSafe;

/**
 * An editor's InputConnection callbacks are delivered on the view's Looper, the same thread as the
 * lifecycle callbacks, so they cannot race with them. They used to be reported because the
 * confinement rule requires the method name to start with "on".
 */
@ThreadSafe
class ImeCallbacks {

  private int mComposingLength;

  static class Editor extends BaseInputConnection {

    private int mSelectionStart;

    Editor(View view) {
      super(view, true);
    }

    @Override
    public boolean commitText(CharSequence text, int newCursorPosition) {
      mSelectionStart = newCursorPosition;
      return true;
    }

    @Override
    public boolean setComposingText(CharSequence text, int newCursorPosition) {
      // reads what commitText writes, on the same Looper
      return mSelectionStart == newCursorPosition;
    }

    @Override
    public boolean finishComposingText() {
      mSelectionStart = 0;
      return true;
    }
  }

  /** An IME callback beside a lifecycle callback: still one thread, so still no race. */
  static class EditorService extends Service {

    private int mState;

    @Override
    public void onCreate() {
      mState = 1;
    }

    @Override
    public IBinder onBind(Intent intent) {
      return null;
    }
  }

  /** A plain method of the same class is NOT confined, so a genuine race is still reported. */
  static class EditorWithWorker extends BaseInputConnection {

    private int mValue;

    EditorWithWorker(View view) {
      super(view, true);
    }

    @Override
    public boolean commitText(CharSequence text, int newCursorPosition) {
      mValue = newCursorPosition;
      return true;
    }

    /** Not an InputConnection method and not a lifecycle method: any thread may call it. */
    public void updateFromBackgroundBad(int value) {
      mValue = value;
    }

    public synchronized int readBad() {
      return mValue;
    }
  }

  /**
   * The control that keeps the rule narrow: binderDied runs on a BINDER POOL thread, not the
   * Looper, and has no "on" prefix. Confining every android.* override would silence this, which
   * is a known true positive.
   */
  @ThreadSafe
  static class BinderDeathWatcher implements IBinder.DeathRecipient {

    private Object mService;

    @Override
    public void binderDied() {
      mService = null;
    }

    public Object ensureConnectedBad() {
      if (mService == null) {
        mService = new Object();
      }
      return mService;
    }
  }

  void unrelatedBad() {
    mComposingLength = 1;
  }

  public synchronized int readComposingLengthBad() {
    return mComposingLength;
  }
}
