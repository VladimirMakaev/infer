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
int scale(unsigned int);

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

// Part of the copy's scope is unreachable because the control flow cannot get
// there, not because the analysis gave up on it. Dead code is not a gap in what
// we know -- the assignment below cannot run on any execution -- so the copy is
// still unnecessary and still reports.
int copy_with_dead_code_in_scope_bad(int n) {
  std::string copied = get_session().getBeginString();
  sink(copied);
  if (n > 0) {
    return copied.size();
  }
  return 0;
  copied = "no execution reaches this"; // dead: no predecessor in the CFG
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

// The shape of the production false positive this guard exists for: two loops
// over the same unsigned bound, with the copy between them. Leaving the first
// loop without iterating forces the bound to zero, which makes the second loop
// -- the code that overwrites the copy -- unreachable on exactly the paths that
// survive, while the paths that do iterate are abandoned above. The copy's
// scope end is reached on the surviving path, so the scope-end gate stays
// quiet; what says to say nothing is the unexplored region inside the scope.
int copy_overwritten_in_unexplored_loop_ok(
    unsigned int iterations, int a, int b, int c, int d, int e, int f) {
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
  for (unsigned int i = 0; i < iterations; i++) {
    acc += scale(i);
  }
  std::string copied = get_session().getBeginString();
  sink(copied);
  for (unsigned int j = 0; j < iterations; j++) {
    copied = get_session().getBeginString();
  }
  return copied.size() + acc;
}
