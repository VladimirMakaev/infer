/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <map>
#include <mutex>
#include <vector>

// with deduplication, a method gets one report per container, at its first
// racy call
namespace containers {

struct Map {
  void erase(int key) {
    std::lock_guard<std::mutex> lock(mutex_);
    map_.erase(key);
  }

  bool find_then_end_bad(int key) {
    auto it = map_.find(key);
    return it != map_.end();
  }

  bool end_then_find_bad(int key) {
    auto end = map_.end();
    return map_.find(key) != end;
  }

  int call_then_at_bad(int key) {
    if (!contains(key)) {
      return 0;
    }
    return map_.at(key);
  }

  int at_then_call_bad(int key) {
    int value = map_.at(key);
    return contains(key) ? value : 0;
  }

 private:
  bool contains(int key) { return map_.find(key) != map_.end(); }

  std::mutex mutex_;
  std::map<int, int> map_;
};

struct Vector {
  void push_back(int x) {
    std::lock_guard<std::mutex> lock(mutex_);
    vec_.push_back(x);
  }

  long end_minus_begin_bad() { return vec_.end() - vec_.begin(); }

 private:
  std::mutex mutex_;
  std::vector<int> vec_;
};

} // namespace containers
