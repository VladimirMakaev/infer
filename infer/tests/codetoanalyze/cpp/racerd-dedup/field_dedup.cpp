/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mutex>
#include <vector>

namespace field_dedup {

class Scalars {
 public:
  void set_ok(int value) {
    std::lock_guard<std::mutex> lock(mutex_);
    first_ = value;
    second_ = value;
  }

  // Both fields reach the caller at the same call site.
  int pair_bad() { return read_pair(); }

  int direct_pair_bad() { return first_ + second_; }

  int pair_locked_ok() {
    std::lock_guard<std::mutex> lock(mutex_);
    return read_pair();
  }

  // Repeated traces for the same field still give one report.
  int repeated_first_bad() {
    int result = read_first();
    return result + read_first_again();
  }

  int repeated_first_locked_ok() {
    std::lock_guard<std::mutex> lock(mutex_);
    return read_first() + read_first();
  }

 private:
  int read_pair() { return first_ + second_; }
  int read_first() { return first_; }
  int read_first_again() { return first_; }

  std::mutex mutex_;
  int first_;
  int second_;
};

class Nested {
 public:
  void set_ok(int value) {
    std::lock_guard<std::mutex> lock(mutex_);
    left_.value = value;
    right_.value = value;
  }

  int pair_bad() { return read_pair(); }

  int pair_locked_ok() {
    std::lock_guard<std::mutex> lock(mutex_);
    return read_pair();
  }

 private:
  struct Cell {
    int value;
  };

  int read_pair() { return left_.value + right_.value; }

  std::mutex mutex_;
  Cell left_;
  Cell right_;
};

class Containers {
 public:
  void set_ok(int value) {
    std::lock_guard<std::mutex> lock(mutex_);
    left_.push_back(value);
    right_.push_back(value);
  }

  int pair_bad() { return read_pair(); }

  int pair_locked_ok() {
    std::lock_guard<std::mutex> lock(mutex_);
    return read_pair();
  }

  int repeated_left_bad() {
    int result = read_left();
    return result + read_left_again();
  }

 private:
  int read_pair() { return left_.size() + right_.size(); }
  int read_left() { return left_.size(); }
  int read_left_again() { return left_.empty(); }

  std::mutex mutex_;
  std::vector<int> left_;
  std::vector<int> right_;
};

} // namespace field_dedup
