/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <string>

struct SessionID {
  const std::string& getBeginString() const { return m_beginString; }
  std::string m_beginString;
};

const SessionID& get_session();
void sink(const std::string&);
[[noreturn]] void always_throws();

// The copy is read and never modified, and the end of its scope is reachable
// only on a path the analyser proves infeasible. Nothing is abandoned here, so
// the copy is unnecessary on every path that was explored.
int copy_scope_end_infeasible_bad(int n) {
  if (n > 0) {
    std::string copied = get_session().getBeginString();
    sink(copied);
    if (n < 0) {
      return copied.size();
    }
    always_throws();
  }
  return 0;
}

// Same, with the infeasibility coming from an equality rather than a range.
int copy_scope_end_eq_infeasible_bad(int n) {
  if (n == 1) {
    std::string copied = get_session().getBeginString();
    sink(copied);
    if (n == 2) {
      return copied.size();
    }
    always_throws();
  }
  return 0;
}

// The same shape, but with enough branching before the copy to exceed the
// disjunct limit. Here states really are abandoned, so one of them may have
// modified the copy without us seeing it: stay silent.
int copy_scope_end_with_dropped_disjuncts_ok(
    int n, int a, int b, int c, int d, int e, int f) {
  int acc = 0;
  if (a) {
    acc += 1;
  }
  if (b) {
    acc += 2;
  }
  if (c) {
    acc += 4;
  }
  if (d) {
    acc += 8;
  }
  if (e) {
    acc += 16;
  }
  if (f) {
    acc += 32;
  }
  if (n > 0) {
    std::string copied = get_session().getBeginString();
    sink(copied);
    if (n < 0) {
      return copied.size() + acc;
    }
    always_throws();
  }
  return acc;
}
