/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mutex>

namespace protected_cross_tu {

class Base {
 public:
  void set(int x);

 protected:
  void set_locked_bad(int x);

 private:
  std::mutex mutex_;
  int x_ = 0;
  int last_ = 0;
};

class SafeBase {
 public:
  void set(int x);

 protected:
  void set_locked_ok(int x);
  std::mutex mutex_;

 private:
  int x_ = 0;
  int last_ = 0;
};

// A method of the SAME class defined in another translation unit breaks the "my callers hold the
// lock" contract just as a derived one does, and it is just as invisible to the file where the
// helper is defined: only the recorded call edges name it.
class SameClass {
 public:
  void set(int x);
  void set_unlocked(int x); // defined in protected_cross_tu_same_class.cpp, takes no lock

 protected:
  void set_locked_bad(int x);

 private:
  std::mutex mutex_;
  int x_ = 0;
  int last_ = 0;
};

// A chain whose top is credited in this file: the only call site of outer_bad here holds the
// lock, and inner_bad is reached only from outer_bad. The caller in the other translation unit
// breaks the top of the chain, and the bottom has to lose its credit with it.
class ChainedSameClass {
 public:
  void set(int x);
  void set_unlocked(int x); // defined in protected_cross_tu_same_class.cpp, takes no lock

 protected:
  void outer_bad(int x);
  void inner_bad(int x);

 private:
  std::mutex mutex_;
  int x_ = 0;
  int last_ = 0;
};

// The arms the review's probe used, kept as permanent controls. Two are above under other names:
// SameFileCredited is SafeBase, whose only caller is in this file and holds the lock, and
// BrokenCrossTu is SameClass, whose caller in the other translation unit takes no lock and is the
// one arm that must report.

// CreditedCrossTu: the helper is called with the lock held in this file, so it is credited here.
// It is ALSO called from another translation unit by a protected method that is itself credited,
// middle_ok, whose own caller holds the lock. Checking middle_ok on its own finds no lock held
// and takes the helper's credit away, which is a false positive: whether a caller denies credit
// cannot be decided before that caller's own credit is.
class CreditedCrossTu {
 public:
  void set(int x);
  void set_via_middle(int x);

 protected:
  void middle_ok(int x); // defined in protected_cross_tu_same_class.cpp
  void helper_ok(int x);

 private:
  std::mutex mutex_;
  int x_ = 0;
  int last_ = 0;
};

// DeepCreditedCrossTu: the same, with two credited levels in the other translation unit instead
// of one, so the credit has to be carried across twice. This is the arm that rules out looking
// only one level past the file being analysed.
class DeepCreditedCrossTu {
 public:
  void set(int x);
  void set_via_outer(int x);

 protected:
  void outer_ok(int x);  // defined in protected_cross_tu_same_class.cpp
  void middle_ok(int x); // defined in protected_cross_tu_same_class.cpp
  void helper_ok(int x);

 private:
  std::mutex mutex_;
  int x_ = 0;
  int last_ = 0;
};

// CreditedOnlyCrossTu: the helper has NO caller in the file that defines it, only the credited
// one in the other translation unit. A rule that reads one file's summaries sees no caller at
// all and so never credits it; reading the recorded edges credits it and the report goes away.
// That is this change silencing something its predecessors reported, which is the opposite
// direction from the rest of this file, and it is here so that it is visible as a fixture rather
// than discovered later in a report diff.
class CreditedOnlyCrossTu {
 public:
  void set(int x);

 protected:
  void middle_ok(int x); // defined in protected_cross_tu_same_class.cpp
  void helper_ok(int x);

 private:
  std::mutex mutex_;
  int x_ = 0;
  int last_ = 0;
};

} // namespace protected_cross_tu
