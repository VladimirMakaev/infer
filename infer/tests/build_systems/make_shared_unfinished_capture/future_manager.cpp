/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <memory>

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

// The call to std::make_shared this builds is the one whose declaration the
// capture no longer holds, so the model that matches its name has no formals
// to read. Analysing it used to abort the whole run.
void* build_a_manager() {
  int x = 0;
  Manager manager(&a_callback, &x, nullptr);
  return manager.context_.get();
}

// Reported only if the analysis of the file gets this far.
int unrelated_bad() {
  int* p = nullptr;
  return *p;
}
