/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <vector>

// A stand-in with the container's real interface, in the style this suite already uses for the
// library's F14 iterators in iterator_distance.cpp: the upstream header does not compile on its
// own here. What matters for the models is reproduced exactly -- in particular `iterator` is a
// raw pointer, not a class, so every member that hands one back returns it rather than filling
// in an iterator object the way `std::vector` does.
//
// The storage invariant is the one that produced the reports: a default-constructed container
// holds a null begin pointer, and the range insert returns early when the range is empty, so
// that pointer survives the call.

void* checked_allocate(std::size_t n);

namespace folly {

template <class T, class Allocator = std::allocator<T>>
class fbvector {
  T* b_ = nullptr;
  T* e_ = nullptr;

 public:
  using value_type = T;
  using reference = value_type&;
  using const_reference = const value_type&;
  using iterator = T*;
  using const_iterator = const T*;
  using size_type = std::size_t;
  using reverse_iterator = std::reverse_iterator<iterator>;

  fbvector() = default;
  fbvector(const fbvector& other) {}
  fbvector(std::initializer_list<T> il) {}
  ~fbvector() = default;

  iterator begin() noexcept { return b_; }
  iterator end() noexcept { return e_; }
  const_iterator cbegin() const noexcept { return b_; }
  const_iterator cend() const noexcept { return e_; }

  size_type size() const noexcept { return size_type(e_ - b_); }
  bool empty() const noexcept { return b_ == e_; }
  T* data() noexcept { return b_; }
  reference operator[](size_type n) { return b_[n]; }
  reference front() { return *b_; }

  void clear() noexcept { e_ = b_; }
  void push_back(const T& v) { grow(1); }
  void reserve(size_type n) { grow(n); }

  template <class FIt>
  iterator insert(const_iterator cpos, FIt first, FIt last) {
    std::size_t n = static_cast<std::size_t>(std::distance(first, last));
    if (n == 0) {
      return const_cast<iterator>(cpos);
    }
    T* nb = static_cast<T*>(checked_allocate((size() + n) * sizeof(T)));
    std::size_t i = 0;
    for (T* p = b_; p != e_; ++p, ++i) {
      nb[i] = *p;
    }
    for (FIt it = first; it != last; ++it, ++i) {
      nb[i] = *it;
    }
    b_ = nb;
    e_ = nb + i;
    return b_;
  }

 private:
  void grow(size_type n) {
    T* nb = static_cast<T*>(checked_allocate((size() + n) * sizeof(T)));
    b_ = nb;
    e_ = nb + n;
  }
};

} // namespace folly

namespace fbvector_models {

int sink(int);

// Filling a container from a non-empty range and then reading the first element. Before the
// container was modelled its body was analysed, the default constructor's null begin pointer
// was the only value the analysis had for the storage, and the early return in the range insert
// kept a path on which that null reached the dereference.
int inserted_from_moved_range_ok(std::vector<int>& src) {
  if (src.empty()) {
    return 0;
  }
  folly::fbvector<int> out;
  out.insert(
      out.end(),
      std::make_move_iterator(src.begin()),
      std::make_move_iterator(src.end()));
  return sink(*out.begin());
}

template <class FIt>
static void insert_all(folly::fbvector<int>& out, FIt first, FIt last) {
  out.insert(out.end(), first, last);
}

// The same, with the insert one call deeper: the endpoints reach the callee as formals.
int inserted_through_helper_ok(std::vector<int>& src) {
  if (src.empty()) {
    return 0;
  }
  folly::fbvector<int> out;
  insert_all(
      out,
      std::make_move_iterator(src.begin()),
      std::make_move_iterator(src.end()));
  return sink(*out.begin());
}

// The same again with plain iterators, to show the adaptor was never the cause.
int inserted_from_plain_range_ok(std::vector<int>& src) {
  if (src.empty()) {
    return 0;
  }
  folly::fbvector<int> out;
  out.insert(out.end(), src.begin(), src.end());
  return sink(*out.begin());
}

// The matching std::vector shapes, which have always been silent: the two containers now agree.
int std_vector_inserted_from_moved_range_ok(std::vector<int>& src) {
  if (src.empty()) {
    return 0;
  }
  std::vector<int> out;
  out.insert(
      out.end(),
      std::make_move_iterator(src.begin()),
      std::make_move_iterator(src.end()));
  return sink(*out.begin());
}

int std_vector_empty_begin_ok() {
  std::vector<int> v;
  return sink(*v.begin());
}

// Dereferencing the storage of a container that is known to be empty is a real bug, and
// modelling the container gives it up: the model keeps the container's own storage pointer out
// of the analysis, so there is no null left to find. This is not a new blind spot of this
// change so much as the one std::vector already has -- std_vector_empty_begin_ok above is the
// same program and has never been reported either -- and the two are now consistent.
int FN_never_inserted_bad() {
  folly::fbvector<int> out;
  return sink(*out.begin());
}

int FN_cleared_after_insert_bad(std::vector<int>& src) {
  if (src.empty()) {
    return 0;
  }
  folly::fbvector<int> out;
  out.insert(out.end(), src.begin(), src.end());
  out.clear();
  return sink(*out.begin());
}

// Detection the models add rather than remove: a reference into the storage does not survive a
// reallocation, which the unmodelled container could not express.
int reference_after_push_back_bad() {
  folly::fbvector<int> v;
  v.push_back(1);
  int& r = v[0];
  v.push_back(2);
  return sink(r);
}

int data_then_push_back_bad() {
  folly::fbvector<int> v;
  v.push_back(1);
  int* p = v.data();
  v.push_back(2);
  return sink(*p);
}

int reference_before_push_back_ok() {
  folly::fbvector<int> v;
  v.push_back(1);
  int& r = v[0];
  int x = r;
  v.push_back(2);
  return sink(x);
}

} // namespace fbvector_models
