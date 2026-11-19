/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "protected_cross_tu.h"

namespace protected_cross_tu {

// The unlocked caller lives here rather than next to the helper, so the file that defines
// SameClass::set_locked_bad sees only locked callers and would otherwise drop its accesses.
void SameClass::set_unlocked(int x) { set_locked_bad(x); }

// Breaks the TOP of a chain from another translation unit. inner_bad is never called from here,
// so it keeps its credit only as long as outer_bad keeps its own; losing it has to reach the
// bottom of the chain as well.
void ChainedSameClass::set_unlocked(int x) { outer_bad(x); }

// Credited rather than locked: middle_ok takes no lock of its own, it is credited because its
// only caller, over in the other translation unit, holds one. The helper below it is credited
// through it, so reporting either is a false positive.
void CreditedCrossTu::middle_ok(int x) { helper_ok(x); }

// Two credited levels, both here, so the walk has to leave the helper's file twice.
void DeepCreditedCrossTu::outer_ok(int x) { middle_ok(x); }

void DeepCreditedCrossTu::middle_ok(int x) { helper_ok(x); }

// The only caller the helper has anywhere.
void CreditedOnlyCrossTu::middle_ok(int x) { helper_ok(x); }

} // namespace protected_cross_tu
