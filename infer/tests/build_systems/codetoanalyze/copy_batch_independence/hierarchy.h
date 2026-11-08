/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once
#include <memory>

struct Resource {
  int x;
};

// Its constructor never returns: see context.cpp. That is what makes the leaf constructors
// below have no normal exit once this unit is captured.
struct GraphicsContext {
  void* window_;
  GraphicsContext(void* window);
};

struct RendererBase : GraphicsContext {
  std::shared_ptr<Resource> res_;
  RendererBase(std::shared_ptr<Resource> res);
};

struct CopyIntoBase_bad : RendererBase {
  CopyIntoBase_bad(std::shared_ptr<Resource> res);
};

struct MoveIntoBase_ok : RendererBase {
  MoveIntoBase_ok(std::shared_ptr<Resource> res);
};

// `res` is read again after the base initialiser, so the copy cannot become a move.
struct UsedAfterBase_ok : RendererBase {
  UsedAfterBase_ok(std::shared_ptr<Resource> res);
};
