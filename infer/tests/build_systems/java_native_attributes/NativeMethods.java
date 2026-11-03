/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package codetoanalyze.java.infer;

public abstract class NativeMethods {
  public native void nativeInstanceBad();

  public static native void nativeStaticBad();

  public native int nativeOverloadBad(int value);

  public native int nativeOverloadBad(String value);

  public native void nativeArrayBad(int[] values);

  private native void nativePrivateBad();

  public native Object nativeObjectBad(Object value);

  public abstract void abstractOk();

  public void ordinaryOk() {}

  public interface Callback {
    void callbackOk();
  }

  public void callOk(Callback callback) {
    nativeInstanceBad();
    nativeStaticBad();
    nativeOverloadBad(1);
    nativeOverloadBad("value");
    nativeArrayBad(new int[0]);
    nativePrivateBad();
    nativeObjectBad(callback);
    abstractOk();
    ordinaryOk();
    callback.callbackOk();
  }

  public void nullControlBad() {
    Object value = null;
    value.toString();
  }
}
