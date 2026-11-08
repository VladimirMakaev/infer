/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "hierarchy.h"
#include <cstdlib>

extern void* get_display();

static void init_graphics() {
  void* display = get_display();
  if (!display) {
    std::exit(1);
  }
}

static void bind_context() { std::abort(); }

// never returns on any path
GraphicsContext::GraphicsContext(void* window) : window_(window) {
  init_graphics();
  bind_context();
}
