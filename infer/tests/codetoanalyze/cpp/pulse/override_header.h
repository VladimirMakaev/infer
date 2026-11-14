/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>

// Base classes in a HEADER, which is where nearly every real base class lives and which a
// single-file fixture cannot express. Infer's clang frontend does not translate declarations
// that come from an included file unless --headers is passed, so nothing in here is captured:
// these types exist in the derived translation unit only as types to inherit from.
//
// That is what used to break the override suppression. It reconstructed the relation by walking
// the type environment for the base's methods and loading their attributes, and neither is
// available for an uncaptured base, so a REAL override was reported -- and reported erratically,
// because whether the base got translated depended on what else the translation unit did with
// it. Taking the edge from clang removes the dependence entirely: clang resolves the override
// while it still has the declaration in hand.

namespace const_refable {
namespace override_header {

void read_int(int);

// A base DECLARED here and DEFINED in another translation unit.
struct HeaderDeclaredBase {
  virtual ~HeaderDeclaredBase() = default;
  virtual void record(std::shared_ptr<int> p);
};

// A base declared AND defined inline here, so its body exists but still is not captured.
struct HeaderInlineBase {
  virtual ~HeaderInlineBase() = default;
  virtual void record(std::shared_ptr<int> p) { read_int(*p); }
};

// A two-deep chain, both links in the header: the override is against the grandparent.
struct HeaderDeepBase {
  virtual ~HeaderDeepBase() = default;
  virtual void record(std::shared_ptr<int> p) { read_int(*p); }
};

struct HeaderDeepMiddle : HeaderDeepBase {};

// A NON-virtual base in a header. Hiding one of its methods is not overriding, so a derived
// method must keep its report -- the control that stops "the base is in a header" from being
// read as "suppress it".
struct HeaderNonVirtualBase {
  void hides_record_bad(std::shared_ptr<int> p);
};

} // namespace override_header
} // namespace const_refable
