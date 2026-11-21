/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <vector>

namespace vector_iterator_index_read {

struct Target {
  int count = 0;
  void use() { ++count; }
};

// No default member initialiser: this file is also compiled as C++11, where an
// initialiser would stop Holder being an aggregate and Holder{...} would not
// compile.
struct Holder {
  Target* target;
};

// --- scalar elements, read back through every accessor ---

void scalar_index_null_bad() {
  std::vector<Target*> targets;
  targets.push_back(nullptr);
  targets[0]->use();
}

void scalar_at_null_bad() {
  std::vector<Target*> targets;
  targets.push_back(nullptr);
  targets.at(0)->use();
}

void scalar_front_null_bad() {
  std::vector<Target*> targets;
  targets.push_back(nullptr);
  targets.front()->use();
}

void scalar_begin_null_bad() {
  std::vector<Target*> targets;
  targets.push_back(nullptr);
  auto it = targets.begin();
  (*it)->use();
}

void scalar_range_for_null_bad() {
  std::vector<Target*> targets;
  targets.push_back(nullptr);
  for (Target* target : targets) {
    target->use();
  }
}

void scalar_iterator_loop_null_bad() {
  std::vector<Target*> targets;
  targets.push_back(nullptr);
  for (auto it = targets.begin(); it != targets.end(); ++it) {
    (*it)->use();
  }
}

// A non-null element stays non-null through the same accessors.

void scalar_index_nonnull_ok() {
  Target target;
  std::vector<Target*> targets;
  targets.push_back(&target);
  targets[0]->use();
}

void scalar_begin_nonnull_ok() {
  Target target;
  std::vector<Target*> targets;
  targets.push_back(&target);
  auto it = targets.begin();
  (*it)->use();
}

void scalar_range_for_nonnull_ok() {
  Target target;
  std::vector<Target*> targets;
  targets.push_back(&target);
  for (Target* entry : targets) {
    entry->use();
  }
}

// --- struct elements: the field of an element read back through an iterator ---
//
// These need the memberwise element transfer as well, which is a separate
// change: pushing a class object by value leaves the element unwritten, so
// there is no stored field for any accessor to find. The indexed case is in
// the list to show the gap is in the write, not in which cell is read.

void FN_struct_index_null_bad() {
  std::vector<Holder> holders;
  holders.push_back(Holder{nullptr});
  holders[0].target->use();
}

void FN_struct_begin_null_bad() {
  std::vector<Holder> holders;
  holders.push_back(Holder{nullptr});
  auto it = holders.begin();
  it->target->use();
}

void FN_struct_range_for_null_bad() {
  std::vector<Holder> holders;
  holders.push_back(Holder{nullptr});
  for (const Holder& holder : holders) {
    holder.target->use();
  }
}

// Silent today because the element is unwritten rather than because the value
// is known non-null; it becomes meaningful once the transfer lands.
void struct_range_for_nonnull_ok() {
  Target target;
  std::vector<Holder> holders;
  holders.push_back(Holder{&target});
  for (const Holder& holder : holders) {
    holder.target->use();
  }
}

// --- traversal: the index has to follow ++ and -- ---

void traversal_second_element_null_bad() {
  Target target;
  std::vector<Target*> targets;
  targets.push_back(&target);
  targets.push_back(nullptr);
  auto it = targets.begin();
  ++it;
  (*it)->use();
}

void traversal_first_element_nonnull_ok() {
  Target target;
  std::vector<Target*> targets;
  targets.push_back(&target);
  targets.push_back(nullptr);
  auto it = targets.begin();
  (*it)->use();
}

void traversal_step_back_nonnull_ok() {
  Target target;
  std::vector<Target*> targets;
  targets.push_back(&target);
  targets.push_back(nullptr);
  auto it = targets.begin();
  ++it;
  --it;
  (*it)->use();
}

// --- invalidation controls: these must keep reporting ---

void invalidated_by_push_back_bad() {
  Target target;
  std::vector<Target*> targets;
  targets.push_back(&target);
  auto it = targets.begin();
  targets.push_back(&target);
  (*it)->use();
}

void invalidated_by_clear_bad() {
  Target target;
  std::vector<Target*> targets;
  targets.push_back(&target);
  auto it = targets.begin();
  targets.clear();
  (*it)->use();
}

// A raw pointer into the array is dereferenced directly rather than through an
// array access, so it does not reach the element cell the accessors above share.
void FN_scalar_data_null_bad() {
  std::vector<Target*> targets;
  targets.push_back(nullptr);
  (*targets.data())->use();
}

} // namespace vector_iterator_index_read
