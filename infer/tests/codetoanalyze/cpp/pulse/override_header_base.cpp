/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// The out-of-line definitions of the header-declared bases. They live in their OWN translation
// unit on purpose: that is the ordinary C++ arrangement, and it is the one the old suppression
// could not see. The bases are reported here, where they are defined and where their authors
// can act on the advice -- a virtual method that overrides nothing owns its signature.

#include "override_header.h"

namespace const_refable {
namespace override_header {

void HeaderDeclaredBase::record(std::shared_ptr<int> p) { read_int(*p); }

void HeaderNonVirtualBase::hides_record_bad(std::shared_ptr<int> p) {
  read_int(*p + 1);
}

} // namespace override_header
} // namespace const_refable
