/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Recording the READ is the entire reason a discarded-value expression `(void) e` is translated
// to a call to a frontend builtin at all. That builtin is now
// [__infer_skip_discard_value] rather than [__infer_skip], so liveness has to recognise the new
// name: if it did not, every one of these would become a DEAD_STORE. None of them should report.

namespace void_cast_read {

void sink(int);

// the plain idiom: the store is read by the cast, so it is not dead
void void_cast_reads_the_store_ok() {
  int x = 42;
  (void)x;
}

void static_cast_to_void_reads_the_store_ok() {
  int x = 42;
  static_cast<void>(x);
}

#define VOID_CAST_UNUSED(X) \
  do {                      \
    (void)(X);              \
  } while (0)

void void_cast_macro_reads_the_store_ok() {
  int x = 42;
  VOID_CAST_UNUSED(x);
}

void void_cast_reads_a_pointer_ok() {
  int y = 1;
  int* p = &y;
  (void)p;
}

// a genuinely dead store, to prove the suite is still able to report one here
void real_dead_store_bad() {
  int x = 42;
  x = 7;
  sink(x);
}

} // namespace void_cast_read
