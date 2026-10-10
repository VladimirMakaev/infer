/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstddef>
#include <iterator>
#include <type_traits>
#include <vector>

// Representative layouts of the library's forward iterators. Values are visited
// in reverse storage order; the past-the-end position is null, not a pointer one
// element before the array. Their distance cannot be a raw pointer subtraction.
namespace folly {
namespace f14 {
namespace detail {
template <typename Ptr>
class VectorContainerIterator {
  Ptr current_;
  Ptr lowest_;

 public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = typename std::remove_cv<typename std::remove_pointer<Ptr>::type>::type;
  using difference_type = std::ptrdiff_t;
  using pointer = Ptr;
  using reference = value_type&;

  VectorContainerIterator(Ptr current = nullptr, Ptr lowest = nullptr)
      : current_(current), lowest_(lowest) {}
  VectorContainerIterator(const VectorContainerIterator&) = default;
  VectorContainerIterator& operator=(const VectorContainerIterator&) = default;
  reference operator*() const { return *current_; }
  VectorContainerIterator& operator++() {
    if (current_ == lowest_)
      current_ = nullptr;
    else
      --current_;
    return *this;
  }
  friend bool operator==(VectorContainerIterator a, VectorContainerIterator b) {
    return a.current_ == b.current_;
  }
  friend bool operator!=(VectorContainerIterator a, VectorContainerIterator b) {
    return !(a == b);
  }
};

template <typename Ptr>
struct DistanceCursor {
  Ptr itemPtr_;
  std::size_t index_;
};

template <typename Ptr>
class ValueContainerIterator {
  DistanceCursor<Ptr> underlying_;

 public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = typename std::remove_cv<typename std::remove_pointer<Ptr>::type>::type;
  using difference_type = std::ptrdiff_t;
  using pointer = Ptr;
  using reference = value_type&;

  explicit ValueContainerIterator(Ptr value = nullptr) : underlying_{value, 0} {}
  ValueContainerIterator(const ValueContainerIterator&) = default;
  ValueContainerIterator& operator=(const ValueContainerIterator&) = default;
  reference operator*() const { return *underlying_.itemPtr_; }
  ValueContainerIterator& operator++() {
    underlying_.itemPtr_ = nullptr;
    return *this;
  }
  friend bool operator==(ValueContainerIterator a, ValueContainerIterator b) {
    return a.underlying_.itemPtr_ == b.underlying_.itemPtr_;
  }
  friend bool operator!=(ValueContainerIterator a, ValueContainerIterator b) {
    return !(a == b);
  }
};
} // namespace detail
} // namespace f14
} // namespace folly

namespace iterator_distance {
using Iterator = folly::f14::detail::VectorContainerIterator<int*>;
using ValueIterator = folly::f14::detail::ValueContainerIterator<int*>;

int equal_forward_ok(Iterator first) {
  if (std::distance(first, first) != 0)
    return *static_cast<int*>(nullptr);
  return 0;
}

int copied_forward_ok(Iterator first) {
  auto copy = first;
  if (std::distance(first, copy) != 0)
    return *static_cast<int*>(nullptr);
  return 0;
}

int assigned_forward_ok(Iterator first) {
  Iterator copy;
  copy = first;
  if (std::distance(first, copy) != 0)
    return *static_cast<int*>(nullptr);
  return 0;
}

int copied_position_snapshot_ok() {
  int array[] = {1, 2};
  Iterator first(array + 1, array);
  auto copy = first;
  first = Iterator(array, array);
  if (std::distance(copy, first) == 0)
    return *static_cast<int*>(nullptr);
  return 0;
}

int pointer_initializing_constructor_ok() {
  // An element can itself be an iterator: a pointer argument of this type is
  // still a position initializer, not the outer iterator's copy constructor.
  Iterator array[2];
  using Outer = folly::f14::detail::VectorContainerIterator<Iterator*>;
  Outer first(array + 1, array), end;
  if (first == end)
    return 0;
  if (std::distance(first, end) <= 0)
    return *static_cast<int*>(nullptr);
  return 0;
}

int equal_nested_forward_ok(ValueIterator first) {
  if (std::distance(first, first) != 0)
    return *static_cast<int*>(nullptr);
  return 0;
}

int copied_nested_forward_ok(ValueIterator first) {
  auto copy = first;
  if (std::distance(first, copy) != 0)
    return *static_cast<int*>(nullptr);
  return 0;
}

int nonempty_forward_ok() {
  int array[] = {1, 2};
  Iterator first(array + 1, array), end;
  if (first == end)
    return 0;
  if (std::distance(first, end) <= 0)
    return *static_cast<int*>(nullptr);
  return 0;
}

// The count must agree with the copy-loop endpoints even in a callee summary.
struct RangeBuffer {
  std::size_t size;
  int* storage;
  RangeBuffer(Iterator first, Iterator last)
      : size(std::distance(first, last)), storage(size ? new int[size] : nullptr) {
    for (auto it = first; it != last; ++it)
      *storage = *it;
  }
  ~RangeBuffer() { delete[] storage; }
};

int empty_copy_range_ok(Iterator first) {
  RangeBuffer values(first, first);
  return values.size;
}

int nonempty_copy_range_ok() {
  int array[] = {1, 2};
  RangeBuffer values(Iterator(array + 1, array), Iterator());
  return values.size;
}

int empty_storage_bad() {
  RangeBuffer values{Iterator(), Iterator()};
  return *values.storage;
}

int equal_standard_iterator_ok(std::vector<int>::iterator first) {
  if (std::distance(first, first) != 0)
    return *static_cast<int*>(nullptr);
  return 0;
}

int copied_standard_iterator_ok(std::vector<int>::iterator first) {
  auto copy = first;
  if (std::distance(first, copy) != 0)
    return *static_cast<int*>(nullptr);
  return 0;
}

int reversed_standard_iterator_ok() {
  std::vector<int> values{1, 2, 3};
  auto first = values.begin();
  auto last = first + 2;
  if (std::distance(last, first) >= 0)
    return *static_cast<int*>(nullptr);
  return 0;
}

int unknown_standard_origin_bad(std::vector<int>::iterator first,
                                std::vector<int>::iterator last) {
  if (std::distance(first, last) == 0)
    return *static_cast<int*>(nullptr);
  return 0;
}

struct UnknownIterator {
  using iterator_category = std::forward_iterator_tag;
  using value_type = int;
  using difference_type = std::ptrdiff_t;
  using pointer = int*;
  using reference = int&;
  int* opaque;
  int& operator*() const;
  UnknownIterator& operator++();
  friend bool operator==(UnknownIterator, UnknownIterator);
  friend bool operator!=(UnknownIterator, UnknownIterator);
};

int unknown_forward_zero_bad(UnknownIterator first, UnknownIterator last) {
  if (std::distance(first, last) == 0)
    return *static_cast<int*>(nullptr);
  return 0;
}

int unknown_forward_nonzero_bad(UnknownIterator first, UnknownIterator last) {
  if (std::distance(first, last) != 0)
    return *static_cast<int*>(nullptr);
  return 0;
}

int equal_raw_pointer_ok(const int* first) {
  if (std::distance(first, first) != 0)
    return *static_cast<int*>(nullptr);
  return 0;
}

int raw_pointer_distance_ok() {
  int array[] = {1, 2, 3};
  if (std::distance(array, array + 2) != 2)
    return *static_cast<int*>(nullptr);
  return 0;
}

int null_control_bad() { return *static_cast<int*>(nullptr); }
} // namespace iterator_distance
