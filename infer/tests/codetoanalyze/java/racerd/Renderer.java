/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Minimal stand-in for the ExoPlayer Renderer interface, which is not on the test classpath.
// ExoPlayer drives these callbacks only from the playback thread.
package com.google.android.exoplayer2;

public interface Renderer {
  void render();

  void onPositionReset();
}
