/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <stdint.h>

namespace dead_wrap_exceptions {

uint64_t unknown();
void observe(uint64_t);
void may_throw();

// Normal-flow liveness can insert ExitScope(i) before the call, but a handler
// can still observe the unsigned result. Keep the concrete L1 report.
void catch_zero_bad() {
  uint64_t i = unknown();
  if (i != 0) {
    return;
  }
  try {
    i -= 1;
    may_throw();
  } catch (...) {
    observe(i);
  }
}

// The terminal countdown path wraps and then throws; its handler reads i.
void countdown_catch_bad(uint64_t n) {
  uint64_t i = n;
  try {
    while (i-- > 0) {
    }
    may_throw();
  } catch (...) {
    observe(i);
  }
}

// An exception edge alone does not make the wrapped value observable.
void catch_no_read_Ok() {
  uint64_t i = unknown();
  if (i != 0) {
    return;
  }
  try {
    i -= 1;
    may_throw();
  } catch (...) {
    observe(7);
  }
}

// Keep the intended normal-flow countdown suppression.
void normal_countdown_Ok(uint64_t n) {
  for (uint64_t i = n; i-- > 0;) {
    observe(i);
  }
}

// An overwrite makes the old value dead on normal flow, although i is read later.
void normal_overwrite_Ok() {
  uint64_t i = unknown();
  i -= 1;
  i = 7;
  observe(i);
}

// Aliases made before the wrap still defeat the proof, including in handlers.
void catch_alias_Bad(uint64_t n) {
  uint64_t i = n;
  uint64_t* alias = &i;
  try {
    while (i-- > 0) {
    }
    may_throw();
  } catch (...) {
    observe(*alias);
  }
}

// Suppressing the unused subtraction does not suppress its operand checks.
void operand_overflow_Bad(uint64_t n) {
  int64_t bound = INT64_MAX;
  uint64_t unused = static_cast<uint64_t>(bound + 1) - unknown();
}

} // namespace dead_wrap_exceptions
