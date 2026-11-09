/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Grouping the reports that describe one deadlock does not depend on the
// deduplication option. This directory is analysed with --no-deduplicate (it
// passes --debug-exceptions, which implies it), so everything here is the
// behaviour of the grouping with the option OFF.
//
// The locks here are globals, so there is no class whose methods could be
// searched for the other side of an inversion: the only way two procedures are
// compared is sharing a captured file.

#pragma once

#include <mutex>

namespace dedup_always {

extern std::mutex alpha;
extern std::mutex beta;

// one side each, in two different translation units
void split_alpha_then_beta_bad();
void split_beta_then_alpha_bad();

} // namespace dedup_always
