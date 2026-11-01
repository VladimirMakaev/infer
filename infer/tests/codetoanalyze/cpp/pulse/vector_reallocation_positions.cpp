/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
#include <memory>
#include <vector>

// A vector that is read with a loop, then grown, then read again. Reading it a
// second time starts from the vector, so the second read is of the new buffer
// and is correct, however many times the sequence is repeated.
namespace vector_reallocation_positions {

struct Counter {
  std::vector<int> entries_;

  int total() const {
    int sum = 0;
    for (int entry : entries_) {
      sum += entry;
    }
    return sum;
  }

  void add_after_total() {
    total();
    entries_.push_back(1);
  }

  void add_between_totals() {
    total();
    entries_.push_back(1);
    total();
  }
};

int total_after_two_adds_ok() {
  Counter counter;
  counter.add_after_total();
  counter.add_after_total();
  return counter.total();
}

int add_between_totals_twice_ok() {
  Counter counter;
  counter.add_between_totals();
  counter.add_between_totals();
  return counter.total();
}

// The shape of a command-line parser: the owning vector holds unique pointers,
// a new entry is appended and then the whole vector is walked again.
// The two MEMORY_LEAK_CPP reports this file expects are a separate limitation
// of the container ownership model, which does not see the vector's destructor
// free an element written at a position that is not a constant: they are
// reported on the cases below with and without the past-the-end refresh.
struct Option {
  int value_ = 0;
};

using OptionPtr = std::unique_ptr<Option>;

struct OptionSet {
  std::vector<OptionPtr> options_;

  int sum() const {
    int sum = 0;
    for (const OptionPtr& option : options_) {
      sum += option->value_;
    }
    return sum;
  }

  Option* add() {
    sum();
    options_.emplace_back();
    OptionPtr& slot = options_.back();
    slot.reset(new Option());
    sum();
    return slot.get();
  }
};

int add_two_options_then_sum_ok() {
  OptionSet options;
  options.add();
  options.add();
  return options.sum();
}

// Controls: what really does point into the buffer must still be reported,
// including after the loop that gives the vector a past-the-end position.

int slot_reference_after_add_bad() {
  OptionSet options;
  options.add();
  OptionPtr& slot = options.options_.back();
  options.add();
  return slot->value_;
}

int element_reference_after_add_bad() {
  Counter counter;
  counter.add_after_total();
  int& entry = counter.entries_[0];
  counter.add_after_total();
  return entry;
}

int iterator_after_add_bad() {
  Counter counter;
  counter.add_after_total();
  auto iter = counter.entries_.begin();
  counter.add_after_total();
  return *iter;
}

// The pointee of a unique_ptr element is a separate allocation that the vector
// growing does not move.
int pointee_after_add_ok() {
  OptionSet options;
  Option* option = options.add();
  options.add();
  return option->value_;
}

} // namespace vector_reallocation_positions
