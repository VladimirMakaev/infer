/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
#include <memory>
#include <vector>

struct Node {
  int id;
  Node* next;
};

int unique_ptr_null_bad() {
  std::unique_ptr<Node> x;
  return x->id;
}

int unique_ptr_ref_next_null_bad(std::unique_ptr<Node>& x) {
  x->next = nullptr;
  return x->next->id;
}

struct Holder {
  std::shared_ptr<Node> node;

  int shared_ptr_field_null_bad() {
    node.reset();
    return node->id;
  }
};

int vector_element_null_bad(std::vector<Node*>& v) {
  v[0] = nullptr;
  return v[0]->id;
}

int iterator_invalidated_bad(std::vector<Node>& v) {
  auto it = v.begin();
  v.push_back(Node{1, nullptr});
  return it->id;
}

template <class T>
struct Ptr {
  T* p;
  T* operator->() const { return p; }
};

int user_arrow_operator_next_null_bad(Ptr<Node> x) {
  x.p->next = nullptr;
  return x->next->id;
}
