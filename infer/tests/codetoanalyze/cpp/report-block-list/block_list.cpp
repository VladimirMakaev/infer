/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <stdlib.h>

// Reporting de-duplicates issues that differ only by procedure name -- typically the
// same warning on several instantiations of one template -- and keeps an arbitrary one
// of them. The report block list names procedures, so it has to be applied before
// de-duplication. Applied after, whether an entry does anything depends on which
// instantiation de-duplication happened to keep: it deletes the group when it names the
// one that was kept, and does nothing at all otherwise.
//
// Which instantiation is kept is arbitrary, and the entries in .inferconfig are written
// against the one currently kept, which ../report-block-list-none/issues.exp records
// with no block list in the way.

// The block list names the three instantiations that are not Partially<int>, one of
// which is the one de-duplication keeps. Filtering first leaves Partially<int> as the
// only candidate, so that is the report that comes out.
template <class T>
struct Partially {
  void partial_leak_bad() { char* p = (char*)malloc(sizeof(T)); }
};

void instantiate_partially() {
  Partially<int> i;
  i.partial_leak_bad();
  Partially<double> d;
  d.partial_leak_bad();
  Partially<char> c;
  c.partial_leak_bad();
  Partially<long> l;
  l.partial_leak_bad();
}

// Same shape, but the block list names the instantiation that de-duplication does not
// keep, so it has nothing to delete and the leak is reported either way.
template <class T>
struct PartiallyOther {
  void other_leak_bad() { char* p = (char*)malloc(sizeof(T)); }
};

void instantiate_partially_other() {
  PartiallyOther<int> i;
  i.other_leak_bad();
  PartiallyOther<double> d;
  d.other_leak_bad();
}

// Every instantiation is blocked, so this leak is reported for none of them. It is a
// real leak the block list deliberately hides, not a false negative.
template <class T>
struct Entirely {
  void entirely_blocked_leak() { char* p = (char*)malloc(sizeof(T)); }
};

void instantiate_entirely() {
  Entirely<int> i;
  i.entirely_blocked_leak();
  Entirely<double> d;
  d.entirely_blocked_leak();
}

// No block list entry matches, so de-duplication is left to keep whichever
// instantiation it likes and exactly one report comes out.
template <class T>
struct Never {
  void never_leak_bad() { char* p = (char*)malloc(sizeof(T)); }
};

void instantiate_never() {
  Never<int> i;
  i.never_leak_bad();
  Never<double> d;
  d.never_leak_bad();
}

// Neither a template nor blocked: reported whatever the block list does to the rest.
void plain_leak_bad() { char* p = (char*)malloc(8); }
