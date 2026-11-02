/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <memory>

namespace shared_ptr_make_shared_member_init {

struct CallbackContext {
  enum class Kind { RefCounted, NotRefCounted };

  CallbackContext(Kind kind, void (*callback)(void*, void*), void* c0, void* c1)
      : kind_(kind), callback_(callback), c0_(c0), c1_(c1) {}

  Kind kind_;
  void (*callback_)(void*, void*);
  void* c0_;
  void* c1_;
};

struct Manager {
  // the member initializer is a conditional one of whose branches builds the
  // object with several forwarded arguments: the shape the model reads the
  // constructor's parameter types from
  Manager(void (*callback)(void*, void*), void* c0, void* c1)
      : context_(callback ? std::make_shared<CallbackContext>(
                                CallbackContext::Kind::NotRefCounted,
                                callback,
                                c0,
                                c1)
                          : nullptr) {}

  std::shared_ptr<CallbackContext> context_;
};

void a_callback(void*, void*) {}

int* make_shared_in_member_initializer_ok() {
  int x = 0;
  Manager manager(&a_callback, &x, nullptr);
  return (int*)manager.context_->c0_;
}

int* make_shared_in_member_initializer_bad() {
  // no callback, so the conditional leaves the member empty
  Manager manager(nullptr, nullptr, nullptr);
  return (int*)manager.context_->c0_;
}

} // namespace shared_ptr_make_shared_member_init
