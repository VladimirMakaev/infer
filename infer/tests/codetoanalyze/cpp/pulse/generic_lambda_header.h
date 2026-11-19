/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

// Generic lambdas are C++14; this directory is also built as C++11 (../pulse-11).
#if __cplusplus >= 201402L

// A generic lambda inside an inline function, so the same lambda is translated
// once per translation unit that includes this header, at the same source
// location. Each one has to bind its own captures: the closures must not be
// confused for one another just because they share a name.

struct HdrV1 {
  unsigned rank;
};
struct HdrV2 {
  unsigned rank;
};

inline unsigned hdr_pick(const HdrV1&) { return 10; }
inline unsigned hdr_pick(const HdrV2&) { return 20; }

// Two instantiations, and the write has to reach the caller in every TU that
// includes this.
inline unsigned hdr_generic_lambda_write(bool c) {
  HdrV1 a;
  HdrV2 b;
  unsigned rank = 7;
  auto fill = [&](const auto& v) { rank = hdr_pick(v); };
  if (c) {
    fill(b);
  } else {
    fill(a);
  }
  return rank;
}

#endif // __cplusplus >= 201402L
