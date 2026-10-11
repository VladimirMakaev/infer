/* Copyright (c) Meta Platforms, Inc. and affiliates.
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree. */
#include <vector>
#include <memory>
namespace vector_exception_model {
struct Payload {
  int value;
  static bool fail;
  static int effects;
  explicit Payload(int n) : value(n) {}
  Payload(const Payload& p) : value(p.value) {
    if (fail) {
      ++effects;
      throw 1;
    }
  }
  Payload(Payload&& p) noexcept(false) : value(p.value) {
    if (fail) {
      ++effects;
      throw 1;
    }
  }
};
bool Payload::fail = false;
int Payload::effects = 0;

int strong_exception_ok() {
  Payload::fail = false;
  Payload::effects = 0;
  std::vector<Payload> v;
  v.push_back(Payload(7));
  Payload* p = &v[0];
  Payload::fail = true;
  try {
    v.reserve(v.capacity() + 1);
  } catch (int) {
    if (v.size() != 1 || Payload::effects != 1) {
      int* bad = nullptr;
      return *bad;
    }
  }
  return p->value;
}

int normal_relocation_bad() {
  Payload::fail = false;
  std::vector<Payload> v;
  v.push_back(Payload(7));
  Payload* p = &v[0];
  v.reserve(v.capacity() + 1);
  return p->value;
}

int no_reallocation_no_exception_bad() {
  Payload::fail = false;
  std::vector<Payload> v;
  v.push_back(Payload(7));
  Payload::fail = true;
  try {
    v.reserve(0);
    int* bad = nullptr;
    return *bad;
  } catch (int) {
    return 0;
  }
}
struct MoveSucceeds {
  int value;
  explicit MoveSucceeds(int n) : value(n) {}
  MoveSucceeds(const MoveSucceeds&) { throw 1; }
  MoveSucceeds(MoveSucceeds&& p) noexcept : value(p.value) {}
};

int move_relocation_bad() {
  std::vector<MoveSucceeds> v;
  v.push_back(MoveSucceeds(7));
  MoveSucceeds* p = &v[0];
  try {
    v.reserve(v.capacity() + 1);
  } catch (int) {
  }
  return p->value;
}


int catch_reached_bad() {
  Payload::fail = false;
  std::vector<Payload> v;
  v.push_back(Payload(7));
  Payload::fail = true;
  try {
    v.reserve(v.capacity() + 1);
  } catch (int) {
    int* bad = nullptr;
    return *bad;
  }
  return 0;
}
// Existing limitation: scope destructors require general C++ unwinding support.
int FP_raii_unwind_ok() {
  Payload::fail = false;
  std::vector<Payload> v;
  v.push_back(Payload(7));
  Payload* p = &v[0];
  Payload::fail = true;
  try {
    std::unique_ptr<int> guard(new int(7));
    v.reserve(v.capacity() + 1);
  } catch (int) {}
  return p->value;
}
struct MoveCatches {
  int value;
  explicit MoveCatches(int n) : value(n) {}
  MoveCatches(const MoveCatches&) { throw 1; }
  MoveCatches(MoveCatches&& other) noexcept : value(other.value) {
    try { throw 1; } catch (int) {}
  }
};
int noexcept_internal_catch_bad() {
  std::vector<MoveCatches> v;
  v.push_back(MoveCatches(7));
  MoveCatches* p = &v[0];
  try { v.reserve(v.capacity()+1); } catch (int) { return 0; }
  return p->value;
}

int computed_noop_normal_path_bad() {
  Payload::fail = false;
  std::vector<Payload> v;
  v.push_back(Payload(7));
  Payload::fail = true;
  try { v.reserve(v.size() - 1); int* bad = nullptr; return *bad; }
  catch (int) { return 0; }
}
struct PreferCopy {
  int value;
  static int copies;
  static int moves;
  explicit PreferCopy(int n) : value(n) {}
  PreferCopy(const PreferCopy& other) : value(other.value) { ++copies; throw 1; }
  PreferCopy(PreferCopy&& other) noexcept(false) : value(other.value) {
    ++moves; other.value = 99; throw 1;
  }
};
int PreferCopy::copies = 0;
int PreferCopy::moves = 0;
int copy_choice_preserves_effects_ok() {
  std::vector<PreferCopy> v;
  v.push_back(PreferCopy(7));
  v[0].value = 7;
  PreferCopy* p = &v[0];
  PreferCopy::copies = 0;
  PreferCopy::moves = 0;
  try { v.reserve(v.capacity() + 1); }
  catch (int) {
    if (PreferCopy::copies != 1 || PreferCopy::moves != 0 || p->value != 7) {
      int* bad = nullptr; return *bad;
    }
  }
  return 0;
}


void unsupported_throw_helper() { throw 1; }
struct HelperThrow {
  int value;
  static bool fail;
  explicit HelperThrow(int n) : value(n) {}
  HelperThrow(const HelperThrow& other) : value(other.value) { if (fail) unsupported_throw_helper(); }
};
bool HelperThrow::fail = false;
// Existing unsupported helper exception propagation must retain conservative invalidation.
int FP_unsupported_constructor_exit_ok() {
  HelperThrow::fail = false;
  std::vector<HelperThrow> v;
  v.push_back(HelperThrow(7));
  HelperThrow* p = &v[0];
  HelperThrow::fail = true;
  try { v.reserve(v.capacity() + 1); } catch (int) {}
  return p->value;
}
} // namespace vector_exception_model
