// Copyright (c) Facebook, Inc. and its affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

// A callee in a HEADER whose nullable call and fatal are both here. The point of putting it in a
// header is the report's file: a report's file is the file of the procedure it is attached to,
// and its line comes from the diagnostic, so a report made in a caller about a call that lives
// here prints THIS line number against the CALLER's file -- a line that has nothing to do with
// the call, and in the field was repeatedly a comment or an #include.

#pragma once
#include <cstdlib>

void* headerLookup(const char* name); // listed nullable in the Makefile

struct HeaderExtractor_Bad {
  void* fn_;
  HeaderExtractor_Bad() {
    fn_ = headerLookup("init");
    if (!fn_) {
      abort();
    }
  }
};
