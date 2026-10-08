/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <pthread.h>

// Library headers are not translated in tests, so this file mirrors how
// libstdc++ stores the pointer of std::unique_ptr: in the head of a tuple held
// by std::__uniq_ptr_impl. Do not include C++ standard headers.
namespace std {

template <unsigned long _Idx, typename _Head>
struct _Head_base {
  _Head _M_head_impl;
};

template <typename _Tp>
struct tuple : _Head_base<0, _Tp> {};

template <typename _Tp>
class __uniq_ptr_impl {
 public:
  _Tp*& _M_ptr() { return _M_t._M_head_impl; }
  _Tp* _M_ptr() const { return _M_t._M_head_impl; }

  void reset(_Tp* __p) {
    _Tp* __old = _M_ptr();
    _M_ptr() = __p;
    delete __old;
  }

 private:
  tuple<_Tp*> _M_t;
};

template <typename _Tp>
class unique_ptr {
 public:
  _Tp* operator->() const { return _M_t._M_ptr(); }

  void reset(_Tp* __p = nullptr) { _M_t.reset(__p); }

 private:
  __uniq_ptr_impl<_Tp> _M_t;
};

} // namespace std

namespace unique_ptr_libstdcxx {

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

} // namespace unique_ptr_libstdcxx
