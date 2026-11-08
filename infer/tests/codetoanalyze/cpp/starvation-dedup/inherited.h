/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// A caller that takes no lock of its own, and whose callee takes both, is quiet
// only when that callee is put together with the other side somewhere else.
// Procedures are compared with the procedures of their own captured file, so
// "somewhere else" means the two sides share a file. These are the two cases,
// built the same way apart from how the two helpers are spread over files.
//
// The locks are globals on purpose: that is the shape where the only way the two
// sides meet is the file, because a global belongs to no class whose methods
// could be searched instead.

#pragma once

#include <mutex>

namespace inherited {

// taken in inherited-ab.cpp and inherited-ba.cpp, one helper each
extern std::mutex alpha;
extern std::mutex beta;

// both taken in inherited-pair.cpp
extern std::mutex gamma;
extern std::mutex delta;

void split_alpha_then_beta_bad();
void split_beta_then_alpha_bad();

void joined_gamma_then_delta_bad();
void joined_delta_then_gamma_bad();

} // namespace inherited
