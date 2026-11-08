/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "hierarchy.h"
RendererBase::RendererBase(std::shared_ptr<Resource> res)
    : GraphicsContext(nullptr), res_(std::move(res)) {}
