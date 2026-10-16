/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// A libc++ built with _LIBCPP_ABI_USE_WRAP_ITER_IN_STD_ARRAY gives std::array
// the class iterator it already gives std::vector, so begin and end hand the
// iterator back as an object and the call carries it as a second argument.
// The declarations are written out here rather than included because the
// headers this suite compiles against give std::array a plain pointer.

typedef unsigned long wrap_iter_size_t;

namespace std {

template <class T>
struct __wrap_iter {
  T* __i;

  T& operator*() const { return *__i; }
  bool operator!=(const __wrap_iter& other) const { return __i != other.__i; }
};

template <class T, wrap_iter_size_t N>
struct array {
  T __elems_[N];

  __wrap_iter<T> begin() {
    __wrap_iter<T> it;
    it.__i = __elems_;
    return it;
  }
  __wrap_iter<T> end() {
    __wrap_iter<T> it;
    it.__i = __elems_ + N;
    return it;
  }
};

} // namespace std

unsigned long wrap_iter_begin_Ok(std::array<unsigned long, 2>& a) {
  return *a.begin();
}

bool wrap_iter_end_Ok(std::array<unsigned long, 2>& a,
                      std::__wrap_iter<unsigned long> it) {
  return it != a.end();
}
