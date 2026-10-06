/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

namespace dynamic_dispatch {

struct Widget;

struct Component {
  virtual ~Component() {}
  virtual Widget* asWidget() { return nullptr; }
};

} // namespace dynamic_dispatch
