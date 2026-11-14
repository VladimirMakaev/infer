/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Overrides of bases that live in a header. Every `override` here is accepted by clang, so each
// one really does override and cannot change its parameter types: all of them must be SILENT.
// They were reported before the override edge came from clang, because the base's methods were
// not in the type environment and its attributes were not in the capture.
//
// The three isolation cells matter as much as the plain ones. The old behaviour depended on
// whether something ELSE in the translation unit made the frontend translate the base, so the
// same declaration reported or not according to its neighbours. Each isolation cell uses its
// base in a different way -- not at all, in a sizeof, and through a call -- and all three must
// now agree, which is the property the old path could not provide.

#include "override_header.h"

#include <cstddef>
#include <memory>
#include <string>

namespace const_refable {
namespace override_header {

void read_size(unsigned long);

// --- the base is only inherited from -----------------------------------------------------

struct DerivedOfDeclared : HeaderDeclaredBase {
  void record(std::shared_ptr<int> p) override { read_int(*p + 1); }
};

struct DerivedOfInline : HeaderInlineBase {
  void record(std::shared_ptr<int> p) override { read_int(*p + 2); }
};

struct DerivedOfDeep : HeaderDeepMiddle {
  void record(std::shared_ptr<int> p) override { read_int(*p + 3); }
};

// --- the same override, with the base used three different ways in the same TU ------------

struct IsolationNoUse : HeaderInlineBase {
  void record(std::shared_ptr<int> p) override { read_int(*p + 4); }
};

struct IsolationSizeof : HeaderInlineBase {
  void record(std::shared_ptr<int> p) override {
    read_size(sizeof(HeaderInlineBase));
    read_int(*p + 5);
  }
};

struct IsolationCall : HeaderInlineBase {
  void record(std::shared_ptr<int> p) override {
    HeaderInlineBase::record(p);
    read_int(*p + 6);
  }
};

// --- controls, so "the base is in a header" cannot be read as "suppress it" ----------------

// Hiding a NON-virtual base method is not overriding, whatever file the base is in: clang
// records no override edge, and this declaration owns its signature.
struct HeaderHider : HeaderNonVirtualBase {
  void hides_record_bad(std::shared_ptr<int> p) { read_int(*p + 7); }
};

// A different by-value parameter type is an overload, not an override, even against a
// header-declared virtual base. clang rejects `override` here.
struct HeaderOverload : HeaderInlineBase {
  void record(std::shared_ptr<const int> q_bad) { read_int(*q_bad); }
};

// A method of its own, inheriting nothing: the coarsest proof the checker ran on this file.
struct HeaderOwn {
  void own_record_bad(std::shared_ptr<int> p) { read_int(*p + 8); }
};

} // namespace override_header
} // namespace const_refable
