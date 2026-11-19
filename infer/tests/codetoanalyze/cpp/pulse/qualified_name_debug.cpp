/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Both shapes are valid C++. The _bad shape previously crashed when debug
// printed procedure attributes; it is not a memory-safety error.
namespace qualified_name_debug {
template <typename... Types> struct TypeList {};
struct Table { int (*entry)(void*); };
template <typename Derived, typename Types> class Support;
template <typename Derived, typename... Types>
class Support<Derived, TypeList<Types...>> {
 public:
  static int entry_bad(void*) { return 0; }
  static const Table table;
};
template <typename Derived, typename... Types>
const Table Support<Derived, TypeList<Types...>>::table = {&Support::entry_bad};

class Plain {
 public:
  static int entry_ok(void*) { return 0; }
  static const Table table;
};
const Table Plain::table = {&Plain::entry_ok};
} // namespace qualified_name_debug
