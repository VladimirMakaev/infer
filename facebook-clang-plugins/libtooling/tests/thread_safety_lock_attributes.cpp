/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
class __attribute__((capability("mutex"))) Mutex {
 public:
  void lock() __attribute__((acquire_capability()));
  void unlock() __attribute__((release_capability()));
  bool try_lock() __attribute__((try_acquire_capability(true)));
};

class C {
  Mutex mu;

  void lock() __attribute__((acquire_capability(mu)));
  void lock_shared() __attribute__((acquire_shared_capability(this -> mu)));
  void unlock(Mutex *other) __attribute__((release_capability(mu, other)));
  bool try_lock() __attribute__((try_acquire_capability(false, mu)));
};
