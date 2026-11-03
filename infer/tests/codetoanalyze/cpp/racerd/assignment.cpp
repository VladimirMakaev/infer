/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mutex>
#include <string>
#include <utility>
#include <vector>
#include <atomic>
struct StringCopy_bad {
  std::mutex m;
  std::string s;
  void set(const std::string& v) {
    std::lock_guard<std::mutex> g(m);
    s = v;
  }
  std::string get_bad() { return s; }
  std::string get_locked_ok() {
    std::lock_guard<std::mutex> g(m);
    return s;
  }
};

struct StringCopy_ok {
  std::mutex m;
  std::string s;
  void set(const std::string& v) {
    std::lock_guard<std::mutex> g(m);
    s = v;
  }
  std::string get_ok() {
    std::lock_guard<std::mutex> g(m);
    return s;
  }
};

struct StringMove_bad {
  std::mutex m;
  std::string s;
  void set(std::string&& v) {
    std::lock_guard<std::mutex> g(m);
    s = std::move(v);
  }
  std::string get_bad() { return s; }
  std::string get_locked_ok() {
    std::lock_guard<std::mutex> g(m);
    return s;
  }
};

struct VectorCopy_bad {
  std::mutex m;
  std::vector<int> v;
  void set(const std::vector<int>& input) {
    std::lock_guard<std::mutex> g(m);
    v = input;
  }
  std::vector<int> get_bad() { return v; }
  std::vector<int> get_locked_ok() {
    std::lock_guard<std::mutex> g(m);
    return v;
  }
};

struct VectorMove_bad {
  std::mutex m;
  std::vector<int> v;
  void set(std::vector<int>&& input) {
    std::lock_guard<std::mutex> g(m);
    v = std::move(input);
  }
  std::vector<int> get_bad() { return v; }
  std::vector<int> get_locked_ok() {
    std::lock_guard<std::mutex> g(m);
    return v;
  }
};

struct Value {
  int n;
  Value& operator=(const Value&);
  Value& operator=(Value&&);
  int read() const { return n; }
};

struct UserCopy_bad {
  std::mutex m;
  Value v;
  void set(const Value& input) {
    std::lock_guard<std::mutex> g(m);
    v = input;
  }
  int get_bad() { return v.read(); }
  int get_locked_ok() {
    std::lock_guard<std::mutex> g(m);
    return v.read();
  }
};

struct UserMove_bad {
  std::mutex m;
  Value v;
  void set(Value&& input) {
    std::lock_guard<std::mutex> g(m);
    v = std::move(input);
  }
  int get_bad() { return v.read(); }
  int get_locked_ok() {
    std::lock_guard<std::mutex> g(m);
    return v.read();
  }
};

struct Scalar_bad {
  std::mutex m;
  int n;
  void set(int input) {
    std::lock_guard<std::mutex> g(m);
    n = input;
  }
  int get_bad() { return n; }
};

struct ConstReads_ok {
  std::string s;
  std::size_t get1() const { return s.size(); }
  bool get2() const { return s.empty(); }
};

struct ConstAssignable {
  const ConstAssignable& operator=(const ConstAssignable&) const { return *this; }
};

struct ConstAssignment_ok {
  std::mutex m;
  ConstAssignable v;
  void set(const ConstAssignable& input) {
    std::lock_guard<std::mutex> g(m);
    v = input;
  }
  ConstAssignable get_ok() { return v; }
};

struct InlineValue {
  int n;
  InlineValue& operator=(const InlineValue& input) {
    n = input.n;
    return *this;
  }
};

struct InlineAssignment_bad {
  std::mutex m;
  InlineValue v;
  void set(const InlineValue& input) {
    std::lock_guard<std::mutex> g(m);
    v = input;
  }
  int get_bad() { return v.n; }
  int get_locked_ok() {
    std::lock_guard<std::mutex> g(m);
    return v.n;
  }
};

void local_assignment_ok(const Value& input) {
  Value local;
  local = input;
  local.read();
}

struct NoOpValue {
  int n;
  NoOpValue& operator=(const NoOpValue&) { return *this; }
};

struct NoOpAssignment_ok {
  std::mutex m;
  NoOpValue v;
  void set(const NoOpValue& input) {
    std::lock_guard<std::mutex> g(m);
    v = input;
  }
  int get_ok() { return v.n; }
};

struct InternallyLockedValue {
  std::mutex m;
  int n;
  InternallyLockedValue& operator=(const InternallyLockedValue& input) {
    std::lock_guard<std::mutex> g(m);
    n = input.n;
    return *this;
  }
  int get() {
    std::lock_guard<std::mutex> g(m);
    return n;
  }
};

struct InternallyLockedAssignment_ok {
  InternallyLockedValue v;
  void set(const InternallyLockedValue& input) { v = input; }
  int get_ok() { return v.get(); }
};

struct PointerSlot_bad {
  std::mutex m;
  std::vector<int>* p;
  void reset(std::vector<int>* input) {
    std::lock_guard<std::mutex> g(m);
    p = input;
  }
  void append(int input) {
    std::lock_guard<std::mutex> g(m);
    p->push_back(input);
  }
  // Reading the pointer slot and reading its pointee are two distinct races.
  std::size_t get_bad() { return p->size(); }
  std::size_t get_locked_ok() {
    std::lock_guard<std::mutex> g(m);
    return p->size();
  }
};

struct WriteOnlyAssignment_ok {
  std::mutex m;
  Value v;
  void set(const Value& input) {
    std::lock_guard<std::mutex> g(m);
    set_unlocked(input);
  }
  // Assignment writes the destination; taking its address is not a read of its value.
  void set_unlocked(const Value& input) { v = input; }
};

struct ComputedReceiver_bad {
  std::mutex m;
  Value* values;
  int index;
  void reset(Value* input, int offset) {
    std::lock_guard<std::mutex> g(m);
    values = input;
    index = offset;
  }
  void assign_unlocked_bad(const Value& input) {
    (values + index)->operator=(input);
  }
  void assign_locked_ok(const Value& input) {
    std::lock_guard<std::mutex> g(m);
    (values + index)->operator=(input);
  }
};

struct AtomicBoolAssignment_ok {
  std::mutex m;
  std::atomic<bool> running{true};
  void set(bool input) {
    std::lock_guard<std::mutex> g(m);
    running = input;
  }
  void assign_explicit(bool input) {
    std::lock_guard<std::mutex> g(m);
    running.operator=(input);
  }
  bool get_ok() { return running.load(); }
  bool conversion_ok() { return running; }
};

struct AtomicPointerAssignment_ok {
  std::mutex m;
  std::atomic<int*> pointer{nullptr};
  void set(int* input) {
    std::lock_guard<std::mutex> g(m);
    pointer = input;
  }
  void assign_explicit(int* input) {
    std::lock_guard<std::mutex> g(m);
    pointer.operator=(input);
  }
  int* get_ok() { return pointer.load(); }
  int* conversion_ok() { return pointer; }
};
