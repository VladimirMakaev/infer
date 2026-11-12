/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

import android.app.Activity;
import android.app.Instrumentation;
import android.media.MediaScannerConnection;
import android.net.Uri;
import android.os.Bundle;
import android.support.annotation.UiThread;
import android.support.annotation.WorkerThread;
import java.util.concurrent.Future;

class AndroidBackgroundCallbacks {
  static void block(Future<?> work) {
    try {
      work.get();
    } catch (Exception e) {
    }
  }

  // The framework delivers scan completion on a background thread.
  static class ScanCompletionOk implements MediaScannerConnection.OnScanCompletedListener {
    Future<?> work;

    @Override
    public void onScanCompleted(String path, Uri uri) {
      block(work);
    }
  }

  // Instrumentation starts on its own thread, unlike Activity.onStart.
  static class InstrumentationStartOk extends Instrumentation {
    Future<?> work;

    @Override
    public void onStart() {
      block(work);
    }
  }

  static class ExplicitUiScanBad implements MediaScannerConnection.OnScanCompletedListener {
    Future<?> work;

    @Override
    @UiThread
    public void onScanCompleted(String path, Uri uri) {
      block(work);
    }
  }

  static class ExplicitUiInstrumentationBad extends Instrumentation {
    Future<?> work;

    @Override
    @UiThread
    public void onStart() {
      block(work);
    }
  }

  static class ExplicitWorkerOk extends Instrumentation {
    Future<?> work;

    @Override
    @WorkerThread
    public void onStart() {
      block(work);
    }
  }

  // A direct call runs on the caller's thread, independently of framework dispatch.
  static class MainCallerBad {
    ScanCompletionOk scanner;
    InstrumentationStartOk instrumentation;

    @UiThread
    public void scanBad() {
      scanner.onScanCompleted("", null);
    }

    @UiThread
    public void instrumentationBad() {
      instrumentation.onStart();
    }
  }

  static class ActivityControlBad extends Activity {
    Future<?> work;

    @Override
    public void onCreate(Bundle state) {
      block(work);
    }
  }
}
