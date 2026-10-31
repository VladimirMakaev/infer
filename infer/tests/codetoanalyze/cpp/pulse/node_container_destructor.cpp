/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <set>
#include <vector>

namespace node_container_destructor {

// Using an element after the container that owned it has been destroyed. The
// sequence containers already reported this; the node-based ones now do too.
int set_use_after_destruction_bad() {
  int* p = nullptr;
  {
    std::set<int> s;
    s.insert(1);
    p = (int*)&*s.begin();
  }
  return *p;
}

int vector_use_after_destruction_bad() {
  int* p = nullptr;
  {
    std::vector<int> v;
    v.push_back(1);
    p = &v[0];
  }
  return *p;
}

// Control: used while the container is still alive.
int set_use_before_destruction_ok() {
  std::set<int> s;
  s.insert(1);
  auto it = s.begin();
  return it != s.end() ? 1 : 0;
}

} // namespace node_container_destructor
