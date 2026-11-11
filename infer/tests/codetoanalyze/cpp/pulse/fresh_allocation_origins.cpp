/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstdlib>

namespace fresh_allocation_origins {

struct Node {
  Node* next;
  int value;
};

Node* wrap_precondition_pointer(Node* pre) {
  Node* fresh = static_cast<Node*>(malloc(sizeof(Node)));
  if (fresh == nullptr) {
    exit(1);
  }
  fresh->next = pre;
  return fresh;
}

// Loading a pointer stored in a new block does not make the pointed-to object new.
int stored_precondition_pointer_bad(Node* pre) {
  Node* wrapper = wrap_precondition_pointer(pre);
  free(pre);
  return wrapper->next->value;
}

// The equality is true: freshness must not refute a value loaded through Dereference.
int stored_pointer_equals_precondition_bad(Node* pre) {
  Node* wrapper = wrap_precondition_pointer(pre);
  if (wrapper->next == pre) {
    free(pre);
    return pre->value;
  }
  return 0;
}

int released_fresh_block_bad() {
  Node* fresh = static_cast<Node*>(malloc(sizeof(Node)));
  if (fresh == nullptr) {
    exit(1);
  }
  free(fresh);
  return fresh->value;
}

int live_fresh_field_ok() {
  Node* fresh = static_cast<Node*>(malloc(sizeof(Node)));
  if (fresh == nullptr) {
    exit(1);
  }
  fresh->value = 1;
  int value = fresh->value;
  free(fresh);
  return value;
}

} // namespace fresh_allocation_origins
