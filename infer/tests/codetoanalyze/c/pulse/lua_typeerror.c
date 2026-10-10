/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

typedef struct lua_State lua_State;
// Lua documents that this function never returns, despite its int return type.
// Deliberately omit a noreturn annotation to exercise the Pulse model.
int luaL_typeerror(lua_State*, int, const char*);

// A similar name must not acquire Lua's non-returning contract.
int luaL_typeerror_returns(lua_State* state, int arg, const char* name) {
  return 0;
}

static int* lua_checked_value(lua_State* state, int* value) {
  if (value == 0)
    luaL_typeerror(state, 1, "value");
  return value;
}

int lua_typeerror_checked_null_ok(lua_State* state) {
  return *lua_checked_value(state, 0);
}

int lua_typeerror_direct_ok(lua_State* state) {
  luaL_typeerror(state, 1, "value");
  int* value = 0;
  return *value;
}

int lua_typeerror_nonnull_ok(lua_State* state) {
  int value = 7;
  return *lua_checked_value(state, &value);
}

int lua_typeerror_returning_bad(lua_State* state) {
  luaL_typeerror_returns(state, 1, "value");
  int* value = 0;
  return *value;
}

int lua_typeerror_null_bad(void) {
  int* value = 0;
  return *value;
}
