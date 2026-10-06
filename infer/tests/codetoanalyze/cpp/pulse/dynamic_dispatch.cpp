/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "dynamic_dispatch.h"

namespace dynamic_dispatch {

struct Base {
  virtual int foo() { return 32; }
};

struct Derived : Base {
  int foo() { return 52; }
};

Base getDerived() { return Derived{}; }

Base* getDerivedPtr() { return new Derived(); }

void dispatch_to_Base_bad() {
  Base base{};
  if (base.foo() == 32) {
    int* p = nullptr;
    *p = 42;
  }
}

// FN because we do not track dynamic types unless it's a heap allocation
// initiated by new()
void FN_dispatch_to_Derived_bad() {
  Base derived = getDerived();
  if (derived.foo() == 52) {
    int* p = nullptr;
    *p = 42;
  }
}

void dispatch_to_Derived_ptr_bad() {
  Base* derived = getDerivedPtr();
  if (derived->foo() == 52) {
    int* p = nullptr;
    *p = 42;
  }
}

struct Shape;

struct Node {
  virtual ~Node() {}
  virtual Shape* asShape() { return nullptr; }
};

struct Shape : Node {
  Shape* asShape() override { return this; }
  int sides;
};

// the receiver may be a [Shape], so the call may return non-null
int downcast_unknown_receiver_bad(Node* node) {
  Shape* shape = node->asShape();
  if (shape == nullptr) {
    return -1;
  }
  int* p = nullptr;
  return *p + shape->sides;
}

int downcast_stack_object_bad() {
  Shape shape;
  Node* node = &shape;
  if (node->asShape() == nullptr) {
    return -1;
  }
  int* p = nullptr;
  return *p;
}

int downcast_base_stack_object_ok() {
  Node base;
  Node* node = &base;
  if (node->asShape() == nullptr) {
    return -1;
  }
  int* p = nullptr;
  return *p;
}

struct Resource {
  virtual ~Resource() { release(); }
  virtual void release() {}
};

struct File : Resource {
  void release() override {}
};

void use_file(const File& file);

// the destructor of the const temporary is specialized for its dynamic type
int destroy_const_temporary_bad() {
  use_file(File());
  int* p = nullptr;
  return *p;
}

// objects in fields, array elements and parameters passed by value have exactly their static type
struct NodeHolder {
  Node node;
  Node nodes[2];
};

int downcast_field_ok(NodeHolder* holder) {
  if (holder->node.asShape() == nullptr) {
    return -1;
  }
  int* p = nullptr;
  return *p;
}

int downcast_array_element_ok(NodeHolder* holder) {
  if (holder->nodes[1].asShape() == nullptr) {
    return -1;
  }
  int* p = nullptr;
  return *p;
}

int downcast_by_value_param_ok(Node node) {
  if (node.asShape() == nullptr) {
    return -1;
  }
  int* p = nullptr;
  return *p;
}

int downcast_reference_param_bad(Node& node) {
  if (node.asShape() == nullptr) {
    return -1;
  }
  int* p = nullptr;
  return *p;
}

// [nodes] may point to a [Shape]
int downcast_pointer_subscript_bad(Node* nodes) {
  if (nodes[0].asShape() == nullptr) {
    return -1;
  }
  int* p = nullptr;
  return *p;
}

struct Edge : Node {};

// only the sibling [Shape] overrides [asShape()], so the body in [Node] is used
int downcast_sibling_override_bad(Edge* edge) {
  Shape* shape = edge->asShape();
  return shape->sides;
}

bool is_shape(Node* node) { return node->asShape() != nullptr; }

// [is_shape] is specialized for the dynamic type of [node]
int is_shape_heap_base_object_ok() {
  Node* node = new Node();
  bool shape = is_shape(node);
  delete node;
  if (shape) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}

int is_shape_stack_base_object_ok() {
  Node base;
  if (is_shape(&base)) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}

// [Edge] inherits [asShape()] from [Node]
int is_shape_stack_sibling_object_ok() {
  Edge edge;
  if (is_shape(&edge)) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}

struct Visitor {
  virtual Shape* visit() = 0;
};

struct NullVisitor : Visitor {
  Shape* visit() override { return nullptr; }
};

bool visits_shape(Visitor* visitor) { return visitor->visit() != nullptr; }

// the type of a local object is only recorded when a method with a body is overridden, so the call
// to the pure virtual [visit()] in [visits_shape] is unknown
int FP_visits_shape_stack_object_ok() {
  NullVisitor visitor;
  if (visits_shape(&visitor)) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}

struct Leaf;

struct Tree {
  virtual Leaf* asLeaf() { return nullptr; }
};

// no class overrides [asLeaf()], so its body is still used
int no_override_unknown_receiver_ok(Tree* tree) {
  if (tree->asLeaf() == nullptr) {
    return -1;
  }
  int* p = nullptr;
  return *p;
}

// [Widget] overrides [asWidget()] in another file, unseen here
int FN_downcast_override_in_other_file_bad(Component* component) {
  if (component->asWidget() == nullptr) {
    return -1;
  }
  int* p = nullptr;
  return *p;
}

} // namespace dynamic_dispatch
