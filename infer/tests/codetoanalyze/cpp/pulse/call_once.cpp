/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mutex>

namespace call_once {

struct LazyValue {
  std::once_flag flag;
  int* value = nullptr;
  int* get() {
    std::call_once(flag, [this] {
      static int v;
      value = &v;
    });
    return value;
  }
};

int lazy_value_ok() {
  LazyValue lazy;
  return *lazy.get();
}

struct SelfRef {
  std::once_flag flag;
  int id = 0;
  SelfRef* ref() {
    SelfRef* self = nullptr;
    std::call_once(flag, [&] { self = this; });
    return self;
  }
};

int self_ref_ok() {
  SelfRef s;
  return s.ref()->id;
}

int* global_ptr;

void init_global_ptr() {
  static int v;
  global_ptr = &v;
}

int function_ok() {
  static std::once_flag flag;
  global_ptr = nullptr;
  std::call_once(flag, init_global_ptr);
  return *global_ptr;
}

int function_pointer_ok() {
  static std::once_flag flag;
  global_ptr = nullptr;
  std::call_once(flag, &init_global_ptr);
  return *global_ptr;
}

int function_pointer_variable_ok() {
  static std::once_flag flag;
  void (*init)() = init_global_ptr;
  global_ptr = nullptr;
  std::call_once(flag, init);
  return *global_ptr;
}

int null_set_by_callable_bad() {
  static std::once_flag flag;
  static int v;
  int* p = &v;
  std::call_once(flag, [&] { p = nullptr; });
  return *p;
}

} // namespace call_once
