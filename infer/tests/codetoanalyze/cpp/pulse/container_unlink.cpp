/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
#include <forward_list>
#include <list>

// The unlink paths that are NOT modelled, pinned silent. `pop_front` and `pop_back` needed a model
// because unlinking a node writes the node's successor through the node's own back link, which the
// analysis cannot relate to the sentinel. These paths do not have that problem and must not grow
// it: `remove` and `unique` unlink by splicing, so no successor is written through a node's own
// back link, and a forward list is singly linked, so unlinking its head writes the sentinel's own
// field rather than going through a back pointer. The matched nodes themselves are destroyed --
// `remove` and `unique` splice them into a local list, which destroys them at scope exit -- so
// what these cases rest on is the absence of that back-link write, not the survival of the
// elements.
//
// Each case holds a reference across the call, or calls twice, so that a future model which
// invalidated the elements instead of the one that goes would make these report.
//
// Why each case is `_ok` differs, which matters to anyone modelling `remove` next. `unique` and
// `resize` are unconditionally safe: `unique` keeps the first of each run of duplicates, and
// `resize(4)` only erases from the tail and never empties, so the front always survives. `remove`,
// `remove_if` and the forward-list case are only conditionally safe -- a use-after-free for a
// caller whose front happens to match. They stay correct as `_ok` because the condition is a
// precondition on a symbolic parameter, so such a report would be latent, and latent issues are
// off by default.
//
// The forward-list cases are forward-looking pins: nothing models `forward_list` today, so no
// perturbation of the current container models can make them report, and they cannot be falsified
// until something does.

bool is_odd(int x) { return x % 2 != 0; }

int list_ref_across_remove_ok(std::list<int>& l) {
  if (l.empty()) {
    return 0;
  }
  int& r = l.front();
  l.remove(999);
  return r;
}

int list_ref_across_remove_if_ok(std::list<int>& l) {
  if (l.empty()) {
    return 0;
  }
  int& r = l.front();
  l.remove_if(is_odd);
  return r;
}

int list_ref_across_unique_ok(std::list<int>& l) {
  if (l.empty()) {
    return 0;
  }
  int& r = l.front();
  l.unique();
  return r;
}

int list_ref_across_resize_ok(std::list<int>& l) {
  if (l.empty()) {
    return 0;
  }
  int& r = l.front();
  l.resize(4);
  return r;
}

void list_remove_twice_ok(std::list<int>& l) {
  l.remove(1);
  l.remove(2);
}

void list_unique_then_remove_ok(std::list<int>& l) {
  l.unique();
  l.remove(1);
}

void list_resize_twice_ok(std::list<int>& l) {
  l.resize(4);
  l.resize(2);
}

// A forward list reaches none of this, and is not modelled at all. The held reference matters for
// the day it is: adding it to the container list would make `front()` and `remove()` modelled.
// This is the forward-list counterpart of `list_ref_across_remove_ok`, and it does not cover
// `pop_front`: a canary for that has to hold a reference to an element the pop does NOT remove,
// because `front()` followed by `pop_front()` is a true positive and must report once modelled.
int flist_ref_across_remove_ok(std::forward_list<int>& f) {
  if (f.empty()) {
    return 0;
  }
  int& r = f.front();
  f.remove(999);
  return r;
}

void flist_pop_twice_ok(std::forward_list<int>& f) {
  if (!f.empty()) {
    f.pop_front();
  }
  if (!f.empty()) {
    f.pop_front();
  }
}

void flist_erase_after_twice_ok(std::forward_list<int>& f) {
  auto it = f.before_begin();
  f.erase_after(it);
  f.erase_after(it);
}

void flist_remove_twice_ok(std::forward_list<int>& f) {
  f.remove(1);
  f.remove(2);
}

// The modelled paths, for contrast: erase and clear relink or empty the container and are already
// covered, and popping twice is the shape that needed the model in the first place.
void list_erase_twice_ok(std::list<int>& l) {
  if (l.size() >= 2) {
    auto it = l.begin();
    it = l.erase(it);
    l.erase(it);
  }
}

void list_clear_then_pop_ok(std::list<int>& l) {
  l.clear();
  if (!l.empty()) {
    l.pop_front();
  }
}
