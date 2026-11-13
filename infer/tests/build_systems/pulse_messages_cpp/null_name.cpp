/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Every zero of a path is one abstract value, so a member that holds false and a null pointer
// member share it. The message has to name the pointer that is dereferenced, not the other one.

struct Holder {
  bool flag_{false};
  int* ptr_{nullptr};

  int get() {
    if (flag_) {
      return 0;
    }
    return *ptr_;
  }
};

int zero_member_shares_the_value_bad() {
  Holder h;
  return h.get();
}

// the dereference is in the same function, where the value's origin already names it
int same_function_bad() {
  Holder h;
  return *h.ptr_;
}

// no second zero to confuse it with
struct OnlyPointer {
  int* ptr_{nullptr};

  int get() { return *ptr_; }
};

int only_pointer_bad() {
  OnlyPointer h;
  return h.get();
}

// the callee dereferences a pointer it was handed, so there is no member to name
int deref_param(int* q) { return *q; }

int null_argument_bad() { return deref_param(nullptr); }
