/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

int* getPtr() { return nullptr; }

bool compareWithNullRef(int* p, const decltype(nullptr)& n) { return p != n; }

bool compareWithNullValue(int* p, decltype(nullptr) n) { return p == n; }

struct WithMember {
  int f;
};

int WithMember::*memberPtrFromNull(const decltype(nullptr)& n) { return n; }
