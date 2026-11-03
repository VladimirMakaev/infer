/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <pthread.h>

namespace pthread {

// the constructor is in recursive_mutex.cpp
class RecursiveMutex {
 public:
  RecursiveMutex();
  void lock() { pthread_mutex_lock(&mutex_); }
  void unlock() { pthread_mutex_unlock(&mutex_); }

 private:
  pthread_mutex_t mutex_;
};

// the constructor delegates the initialisation to a helper that reaches the
// mutex through [this] rather than being handed a pointer to it
class DelegatingRecursiveMutex {
 public:
  DelegatingRecursiveMutex();
  void lock() { pthread_mutex_lock(&mutex_); }
  void unlock() { pthread_mutex_unlock(&mutex_); }

 private:
  void initRecursive();
  pthread_mutex_t mutex_;
};

// the same, but not recursive: the control for DelegatingRecursiveMutex
class DelegatingMutex {
 public:
  DelegatingMutex();
  void lock() { pthread_mutex_lock(&mutex_); }
  void unlock() { pthread_mutex_unlock(&mutex_); }

 private:
  void initDefault();
  pthread_mutex_t mutex_;
};

// owns the mutex but locks it through a pointer to it
class PointerRecursiveMutex {
 public:
  PointerRecursiveMutex();
  void lock() { pthread_mutex_lock(mutex_); }
  void unlock() { pthread_mutex_unlock(mutex_); }

 private:
  pthread_mutex_t storage_;
  pthread_mutex_t* mutex_;
};

// the same, but not recursive: the control for PointerRecursiveMutex
class PointerMutex {
 public:
  PointerMutex();
  void lock() { pthread_mutex_lock(mutex_); }
  void unlock() { pthread_mutex_unlock(mutex_); }

 private:
  pthread_mutex_t storage_;
  pthread_mutex_t* mutex_;
};

} // namespace pthread
