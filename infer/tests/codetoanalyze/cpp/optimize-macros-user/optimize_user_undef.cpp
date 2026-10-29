/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Built with -O2 and an explicit -U__OPTIMIZE__. The real compile has the macro
// undefined, so capture must leave it undefined too: restoring the level's
// macros must not overrule a macro the command names itself.
#if defined(__OPTIMIZE__)
#error "capture re-defined a macro the command explicitly undefined"
#endif

int user_undef_is_respected_bad() {
  int* p = nullptr;
  return *p;
}
