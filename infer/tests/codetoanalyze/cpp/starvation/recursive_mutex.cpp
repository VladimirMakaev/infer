/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "recursive_mutex.h"

namespace pthread {

RecursiveMutex::RecursiveMutex() {
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&mutex_, &attr);
  pthread_mutexattr_destroy(&attr);
}

void DelegatingRecursiveMutex::initRecursive() {
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&mutex_, &attr);
  pthread_mutexattr_destroy(&attr);
}

DelegatingRecursiveMutex::DelegatingRecursiveMutex() { initRecursive(); }

void DelegatingMutex::initDefault() { pthread_mutex_init(&mutex_, nullptr); }

DelegatingMutex::DelegatingMutex() { initDefault(); }

PointerRecursiveMutex::PointerRecursiveMutex() {
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&storage_, &attr);
  pthread_mutexattr_destroy(&attr);
  mutex_ = &storage_;
}

PointerMutex::PointerMutex() {
  pthread_mutex_init(&storage_, nullptr);
  mutex_ = &storage_;
}

} // namespace pthread
