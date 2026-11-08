/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "hierarchy.h"

// `res` is copied into the intermediate that initialises the base, and is never used again, so
// the copy can be a move. The report must not depend on whether the two units holding the base
// constructors happen to be captured alongside this one.
CopyIntoBase_bad::CopyIntoBase_bad(std::shared_ptr<Resource> res) : RendererBase(res) {}

MoveIntoBase_ok::MoveIntoBase_ok(std::shared_ptr<Resource> res)
    : RendererBase(std::move(res)) {}

static int g_read;

static void read_resource(const std::shared_ptr<Resource>& r) {
  if (r) {
    g_read = r->x;
  }
}

// The copy cannot become a move: `res` is read after the base initialiser. Once the two base
// units are captured this constructor has no normal exit, so the read is never analysed and
// the decision has to be made from the shape of the procedure instead. Silent in all four.
UsedAfterBase_ok::UsedAfterBase_ok(std::shared_ptr<Resource> res) : RendererBase(res) {
  read_resource(res);
}
