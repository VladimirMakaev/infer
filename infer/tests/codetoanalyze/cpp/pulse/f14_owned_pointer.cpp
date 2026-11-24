/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <memory>
#include <utility>

// Declaration-only API surface: the model, rather than a surrogate map body, handles calls.
namespace folly {
template <class Key, class = void> struct HeterogeneousAccessHash {};
template <class Key, class = void> struct HeterogeneousAccessEqualTo {};
namespace f14 {
namespace detail {
template <class Value> struct ValueContainerIterator {
  Value* operator->() const;
};
template <class Key, class Value> struct F14BasicMap {
  Value& operator[](const Key&);
  ValueContainerIterator<std::pair<const Key, Value>> begin();
  ValueContainerIterator<std::pair<const Key, Value>> find(const Key&);
  void clear();
  void rehash(unsigned);
  void erase(ValueContainerIterator<std::pair<const Key, Value>>);
};
} // namespace detail
} // namespace f14

template <class Key, class Value, class Hash = HeterogeneousAccessHash<Key>,
          class Equal = HeterogeneousAccessEqualTo<Key>,
          class Allocator = std::allocator<std::pair<const Key, Value>>>
struct F14FastMap : f14::detail::F14BasicMap<Key, Value> {
  F14FastMap();
  ~F14FastMap();
};
template <class Key, class Value, class Hash = HeterogeneousAccessHash<Key>,
          class Equal = HeterogeneousAccessEqualTo<Key>,
          class Allocator = std::allocator<std::pair<const Key, Value>>>
struct F14ValueMap : f14::detail::F14BasicMap<Key, Value> {
  F14ValueMap();
  ~F14ValueMap();
};
template <class Key, class Value, class Hash = HeterogeneousAccessHash<Key>,
          class Equal = HeterogeneousAccessEqualTo<Key>,
          class Allocator = std::allocator<std::pair<const Key, Value>>>
struct F14VectorMap : f14::detail::F14BasicMap<Key, Value> {
  F14VectorMap();
  ~F14VectorMap();
};
} // namespace folly

void fast_map_pointee_leak_bad() {
  folly::F14FastMap<int, int*> map;
  map[1] = new int(7);
}

void value_map_pointee_leak_bad() {
  folly::F14ValueMap<int, int*> map;
  map[1] = new int(7);
}

void vector_map_pointee_leak_bad() {
  folly::F14VectorMap<int, int*> map;
  map[1] = new int(7);
}

void raw_pointer_cleanup_ok() {
  folly::F14FastMap<int, int*> map;
  map[1] = new int(7);
  delete map[1];
}

void saved_alias_cleanup_ok() {
  int* pointer;
  {
    folly::F14FastMap<int, int*> map;
    map[1] = new int(7);
    pointer = map[1];
  }
  // The map destroys its entry, while the separately held pointee remains alive.
  *pointer = 8;
  delete pointer;
}

void borrowed_pointer_ok(int* pointer) {
  folly::F14FastMap<int, int*> map;
  map[1] = pointer;
}

void float_key_pointee_leak_bad() {
  folly::F14FastMap<double, int*> map;
  double key = 1.0;
  map[key] = new int(7);
}

void float_key_cleanup_ok() {
  folly::F14FastMap<double, int*> map;
  double key = 1.0;
  map[key] = new int(7);
  delete map[key];
}

void iterator_after_clear_bad() {
  folly::F14FastMap<int, int*> map;
  map[1] = nullptr;
  auto iterator = map.begin();
  map.clear();
  delete iterator->second;
}

void iterator_after_rehash_bad() {
  folly::F14FastMap<int, int*> map;
  map[1] = nullptr;
  auto iterator = map.begin();
  map.rehash(64);
  delete iterator->second;
}

void storage_after_destruction_bad() {
  int** slot;
  {
    folly::F14FastMap<int, int*> map;
    map[1] = nullptr;
    slot = &map[1];
  }
  delete *slot;
}

struct CustomHash {};
struct CustomEqual {};
template <class Value> struct CustomAllocator {};

// Unsupported policies retain the existing analysis path; they are not claimed as no-op dtors.
void FN_custom_hash_pointee_leak_bad() {
  folly::F14FastMap<int, int*, CustomHash> map;
  map[1] = new int(7);
}

void FN_custom_equal_pointee_leak_bad() {
  folly::F14FastMap<int, int*, folly::HeterogeneousAccessHash<int>, CustomEqual> map;
  map[1] = new int(7);
}

void FN_custom_allocator_pointee_leak_bad() {
  folly::F14FastMap<int, int*, folly::HeterogeneousAccessHash<int>,
                    folly::HeterogeneousAccessEqualTo<int>,
                    CustomAllocator<std::pair<const int, int*>>>
      map;
  map[1] = new int(7);
}

void owning_value_cleanup_ok() {
  folly::F14FastMap<int, std::unique_ptr<int>> map;
  map[1].reset(new int(7));
}

// Known lookup/erase and class-value gaps are separate from raw-pointee destruction.
int FN_unchecked_find_bad() {
  folly::F14FastMap<int, int*> map;
  return *map.find(1)->second;
}

void FN_iterator_after_erase_bad() {
  folly::F14FastMap<int, int*> map;
  map[1] = nullptr;
  auto iterator = map.begin();
  map.erase(iterator);
  delete iterator->second;
}

struct NonOwner {
  int* pointer;
  ~NonOwner() {}
};

void FN_nonowner_pointee_leak_bad() {
  folly::F14FastMap<int, NonOwner> map;
  map[1].pointer = new int(7);
}
