/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

// These declarations test Pulse's standard-library models, not a library implementation.
// The type environment retains this class, but no eligible constructor has the shared-pointer
// reference formals that container insertion needs. The raw-pointer constructor cannot match,
// the copy constructor is deleted, and move construction is absent. The explicit destructor
// keeps the destructor-effects precondition active.
// Use -nostdinc++ so real headers cannot supply an alternative constructor.
namespace std {
template <class T> class shared_ptr {
 public:
  explicit shared_ptr(T*);
  shared_ptr(const shared_ptr&) = delete;
  shared_ptr& operator=(shared_ptr&&);
  ~shared_ptr();
  explicit operator bool() const;
  long use_count() const;
  T* get() const;
  T& operator*() const;
  void reset();
};

template <class T> class vector {
 public:
  vector();
  ~vector();
  void push_back(const T&);
  void push_back(T&&);
  template <class U> T& emplace_back(U&&);
  void clear();
};

template <class T> struct remove_reference { using type = T; };
template <class T> struct remove_reference<T&> { using type = T; };
template <class T> struct remove_reference<T&&> { using type = T; };
template <class T> typename remove_reference<T>::type&& move(T&& value) {
  return static_cast<typename remove_reference<T>::type&&>(value);
}
} // namespace std
