/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

struct lua_State;
// No noreturn annotation: Lua's int-returning declaration needs a Pulse model.
extern "C" int luaL_typeerror(lua_State*, int, const char*);

// Similar names are ordinary returning functions, not Lua error APIs.
extern "C" int luaL_typeerror_returns(lua_State*, int, const char*) {
  return 0;
}

namespace lua_nonreturning {
int* checked_value(lua_State* state, int* value) {
  if (value == nullptr)
    luaL_typeerror(state, 1, "value");
  return value;
}

int checked_null_ok(lua_State* state) { return *checked_value(state, nullptr); }

int direct_ok(lua_State* state) {
  luaL_typeerror(state, 1, "value");
  int* value = nullptr;
  return *value;
}

int nonnull_ok(lua_State* state) {
  int value = 7;
  return *checked_value(state, &value);
}

int returning_bad(lua_State* state) {
  luaL_typeerror_returns(state, 1, "value");
  int* value = nullptr;
  return *value;
}

int null_bad() {
  int* value = nullptr;
  return *value;
}
} // namespace lua_nonreturning
