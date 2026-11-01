/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Computing the escaped formals used to assume that the first actual of a direct call is a
// receiver, and that a receiver is a pointer. Neither holds: a free function has no receiver, and
// a by-value formal is not a pointer, so a scalar formal passed first made the analysis abort.
// Two things are needed together, which is why either one alone looks harmless:
//   * a guard formal that is unlocked, which is what forces the escaped formals to be computed;
//   * a non-pointer formal used as the first actual of a call.
// Nothing here should be reported; the point is that the analysis finishes.

#include <mutex>

namespace escaped_formals_scalar {

void take_scalar(unsigned int x);
void take_enum(int x);

enum Stream { kFirst, kSecond };

class Host {
 public:
  // the shape that aborted: a scalar formal first, and a guard formal unlocked
  void stop_scalar_ok(std::unique_lock<std::mutex>& guard, unsigned int stream) {
    take_scalar(stream);
    guard.unlock();
  }

  // the same with an enum, which is how it arose in practice
  void stop_enum_ok(std::unique_lock<std::mutex>& guard, Stream stream) {
    take_enum(stream);
    guard.unlock();
  }

  // a by-value struct formal: also not a pointer
  struct Small {
    int a;
  };
  void stop_struct_ok(std::unique_lock<std::mutex>& guard, Small s) {
    take_scalar(s.a);
    guard.unlock();
  }

  // the receiver case the predicate exists for: a pointer formal to the callee's own class is
  // kept rather than counted as escaping
  void stop_receiver_ok(std::unique_lock<std::mutex>& guard, Host* other) {
    other->helper();
    guard.unlock();
  }

  void helper() {}
};

} // namespace escaped_formals_scalar
