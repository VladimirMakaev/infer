/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "dynamic_dispatch.h"

namespace dynamic_dispatch {

struct Widget : Component {
  Widget* asWidget() override { return this; }
};

Component* make_widget() { return new Widget(); }

} // namespace dynamic_dispatch
