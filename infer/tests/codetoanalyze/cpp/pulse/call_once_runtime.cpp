/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Mirrors how libc++ implements std::call_once on top of the runtime function
// std::__call_once when it is translated. Do not include C++ standard headers.
namespace std {
void __call_once(volatile unsigned long& flag, void* arg, void (*fn)(void*));
}

namespace call_once_runtime {

struct once_flag {
  unsigned long state = 0;
};

template <class F>
struct param {
  F& f;
};

template <class F>
void proxy(void* p) {
  static_cast<param<F>*>(p)->f();
}

template <class F>
void call_once(once_flag& flag, F&& f) {
  if (flag.state != ~0ul) {
    param<F> p{f};
    std::__call_once(flag.state, &p, &proxy<F>);
  }
}

struct LazyValue {
  once_flag flag;
  int* value = nullptr;
  int* get() {
    call_once(flag, [this] {
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
  once_flag flag;
  int id = 0;
  SelfRef* ref() {
    SelfRef* self = nullptr;
    call_once(flag, [&] { self = this; });
    return self;
  }
};

int self_ref_ok() {
  SelfRef s;
  return s.ref()->id;
}

int null_set_by_callable_bad() {
  once_flag flag;
  static int v;
  int* p = &v;
  call_once(flag, [&] { p = nullptr; });
  return *p;
}

} // namespace call_once_runtime
