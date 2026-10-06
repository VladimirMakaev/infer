/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

namespace liveness_options {

class Guard {};

class NotAGuard {};

struct Dangerous {
  ~Dangerous() {}
};

struct NotDangerous {
  ~NotDangerous() {}
};

void scope_guard_ok() { Guard guard; }

void not_scope_guard_bad() { NotAGuard guard; }

void dangerous_class_bad() { auto d = Dangerous(); }

void not_dangerous_class_ok() { auto d = NotDangerous(); }

} // namespace liveness_options
