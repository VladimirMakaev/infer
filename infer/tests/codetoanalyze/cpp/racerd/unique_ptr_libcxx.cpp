/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <pthread.h>

// Library headers are not translated in tests, so this file mirrors how older
// versions of libc++ store the pointer of std::unique_ptr: in the first
// element of a compressed pair. Do not include C++ standard headers.
namespace std {
inline namespace __1 {

template <class _Tp, int _Idx>
struct __compressed_pair_elem {
  _Tp& __get() { return __value_; }
  const _Tp& __get() const { return __value_; }

  _Tp __value_;
};

template <class _T1>
struct __compressed_pair : private __compressed_pair_elem<_T1, 0> {
  _T1& first() {
    return static_cast<__compressed_pair_elem<_T1, 0>&>(*this).__get();
  }
  const _T1& first() const {
    return static_cast<const __compressed_pair_elem<_T1, 0>&>(*this).__get();
  }
};

template <class _Tp>
class unique_ptr {
 public:
  _Tp* operator->() const { return __ptr_.first(); }

  void reset(_Tp* __p = nullptr) {
    _Tp* __tmp = __ptr_.first();
    __ptr_.first() = __p;
    delete __tmp;
  }

 private:
  __compressed_pair<_Tp*> __ptr_;
};

} // namespace __1
} // namespace std

namespace unique_ptr_libcxx {

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

} // namespace unique_ptr_libcxx
