/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <pthread.h>

// Library headers are not translated in tests, so this file mirrors how recent
// versions of libc++ store the pointer of std::unique_ptr: in an anonymous
// struct. Do not include C++ standard headers.
namespace std {
inline namespace __1 {

template <class _Tp>
class unique_ptr {
 public:
  _Tp* operator->() const { return __ptr_; }

  void reset(_Tp* __p = nullptr) {
    _Tp* __tmp = __ptr_;
    __ptr_ = __p;
    delete __tmp;
  }

 private:
  struct {
    _Tp* __ptr_;
  };
};

} // namespace __1
} // namespace std

namespace unique_ptr_libcxx_anonymous_struct {

struct Node {
  int x;
};

class Owner {
 public:
  void reset_ok() {
    pthread_mutex_lock(&mutex_);
    node_.reset();
    pthread_mutex_unlock(&mutex_);
  }

  void set_ok(int v) {
    pthread_mutex_lock(&mutex_);
    node_->x = v;
    pthread_mutex_unlock(&mutex_);
  }

  // races are reported on `this->node_` and `this->node_->x`
  int get_bad() { return node_->x; }

 private:
  pthread_mutex_t mutex_;
  std::unique_ptr<Node> node_;
};

} // namespace unique_ptr_libcxx_anonymous_struct
