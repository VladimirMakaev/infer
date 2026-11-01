/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
#include <map>
#include <set>
#include <unordered_map>

// A reporter whose contract is to return false so that it can be written inside a condition:
//
//   #define SOFT_VERIFY(c) keep((c) ? true : reportAndReturnFalse(#c))
//   if (SOFT_VERIFY(it != m.end())) { use(it->second); }
//
// It is declared here and defined in another library, so without
// --pulse-model-return-constant in .inferconfig it returns a fresh value and the guarded code
// is analysed with the condition false.
namespace soft_verify {

bool reportAndReturnFalse(const char* condition) noexcept;
bool reportAndReturnUnknown(const char* condition) noexcept;

struct Reporter {
  bool report(const char* condition) noexcept;
};

[[nodiscard]] inline bool keep(bool condition) { return condition; }

#define SOFT_VERIFY(condition) \
  soft_verify::keep((condition) ? true : soft_verify::reportAndReturnFalse(#condition))

#define SOFT_VERIFY_UNKNOWN(condition) \
  soft_verify::keep((condition) ? true : soft_verify::reportAndReturnUnknown(#condition))

#define SOFT_VERIFY_METHOD(reporter, condition) \
  soft_verify::keep((condition) ? true : (reporter).report(#condition))

int unordered_map_find_ok(std::unordered_map<int, int>& map, int key) {
  auto iter = map.find(key);
  if (SOFT_VERIFY(iter != map.end())) {
    return iter->second;
  }
  return 0;
}

int map_find_ok(std::map<int, int>& map, int key) {
  auto iter = map.find(key);
  if (SOFT_VERIFY(iter != map.end())) {
    return iter->second;
  }
  return 0;
}

int set_find_ok(std::set<int>& set, int key) {
  auto iter = set.find(key);
  if (SOFT_VERIFY(iter != set.end())) {
    return *iter;
  }
  return 0;
}

int method_reporter_ok(std::map<int, int>& map, int key, Reporter& reporter) {
  auto iter = map.find(key);
  if (SOFT_VERIFY_METHOD(reporter, iter != map.end())) {
    return iter->second;
  }
  return 0;
}

// Controls: a reporter that is NOT configured says nothing about its result, so the guarded
// code is still analysed with the condition false.
int unconfigured_reporter_bad(std::map<int, int>& map, int key) {
  auto iter = map.find(key);
  if (SOFT_VERIFY_UNKNOWN(iter != map.end())) {
    return iter->second;
  }
  return 0;
}

// Control: the guard holds, so the reporter is irrelevant and the lookup is safe either way.
int plain_guard_ok(std::map<int, int>& map, int key) {
  auto iter = map.find(key);
  if (iter != map.end()) {
    return iter->second;
  }
  return 0;
}

// Control: the modelled reporter returns false, so the branch it guards is taken when the
// condition is false, and a dereference on that branch is still reported.
int guarded_branch_still_checked_bad(std::map<int, int>& map, int key) {
  auto iter = map.find(key);
  if (SOFT_VERIFY(iter != map.end())) {
    return 0;
  }
  return iter->second;
}

// Control: the value the model gives is the one the configuration names, not just "not unknown".
int modelled_false_is_false_ok(std::map<int, int>& map, int key) {
  auto iter = map.find(key);
  if (!SOFT_VERIFY(iter != map.end())) {
    return 0;
  }
  return iter->second;
}

} // namespace soft_verify
