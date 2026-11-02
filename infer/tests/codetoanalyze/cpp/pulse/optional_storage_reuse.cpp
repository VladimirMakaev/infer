/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <optional>

// cpp/pulse-11 recompiles this same source list under -std=c++11, where std::optional does not
// exist. Guard the body the way ph3_optional.cpp does, so the file contributes nothing there.
#if __cplusplus >= 201703L

namespace optional_storage_reuse {

struct Node {
  int v;
  int w;
};

// Reusing an optional's storage renews the lifetime of the CONTAINED OBJECT. It must not renew
// the lifetime of whatever that object POINTED AT: a payload that is a pointer keeps pointing
// at memory the program already freed, and reading it is still a use-after-delete.
//
// These are the true positives that pin the seed of
// PulseModelsImport.Basic.begin_cpp_lifetime_in_storage. Seeding the Dereference target of the
// storage as well as the storage itself silences all three, because on this path the storage IS
// the payload value and its Dereference target is the pointee.

int reuse_after_delete_pointee_emplace_bad() {
  std::optional<Node*> o(new Node{1, 2});
  Node* p = *o;
  delete p;
  o.reset();
  o.emplace(new Node{3, 4});
  return p->v;
}

int reuse_after_delete_pointee_assign_bad() {
  std::optional<Node*> o(new Node{1, 2});
  Node* p = *o;
  delete p;
  o.reset();
  o = new Node{3, 4};
  return p->w;
}

int reuse_after_delete_pointee_emplace_over_bad() {
  std::optional<Node*> o(new Node{1, 2});
  Node* p = *o;
  delete p;
  o.emplace(new Node{3, 4});
  return p->v;
}

// The legal counterparts: reading the NEW payload after the storage is reused is well defined,
// which is what the renewal is for.

int reuse_then_read_new_pointee_ok() {
  std::optional<Node*> o(new Node{1, 2});
  Node* p = *o;
  delete p;
  o.reset();
  o.emplace(new Node{3, 4});
  int v = (*o)->v;
  delete *o;
  return v;
}

int reuse_value_payload_ok() {
  std::optional<Node> o(Node{1, 2});
  o.reset();
  o.emplace(Node{3, 4});
  return o->v;
}

} // namespace optional_storage_reuse

#endif // __cplusplus >= 201703L
