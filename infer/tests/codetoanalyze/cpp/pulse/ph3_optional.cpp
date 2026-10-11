/* Copyright (c) Meta Platforms, Inc. and affiliates.
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree. */
#include <memory>
#include <optional>
#include <stdexcept>
#if __cplusplus >= 201703L
namespace ph3_optional_regressions {
int value_catch_ok() {
  std::optional<int> o;
  try {
    return o.value();
  } catch (const std::bad_optional_access&) {
    return 0;
  }
}
int value_catch_reached_bad() {
  std::optional<int> o;
  try {
    (void)o.value();
  } catch (const std::bad_optional_access&) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}
int value_has_no_normal_continuation_ok() {
  std::optional<int> o;
  try {
    (void)o.value();
    int* p = nullptr;
    return *p;
  } catch (const std::bad_optional_access&) {
    return 0;
  }
}
int value_wrong_catch_bad() {
  std::optional<int> o;
  try {
    (void)o.value();
  } catch (const std::runtime_error&) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}
int value_deleted_optional_bad() {
  auto* o = new std::optional<int>;
  delete o;
  try {
    return o->value();
  } catch (const std::bad_optional_access&) {
    return 0;
  }
}
int value_catch_all_reached_bad() {
  std::optional<int> o;
  try {
    (void)o.value();
  } catch (...) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}
int value_uncaught_bad() {
  std::optional<int> o;
  (void)o.value();
  int* p = nullptr;
  return *p;
}
int empty_deref_bad() {
  std::optional<int> o;
  return *o;
}
int reset_scalar_alias_bad() {
  std::optional<int> o(7);
  int* p = &*o;
  o.reset();
  return *p;
}
int reset_owned_alias_bad() {
  std::optional<std::unique_ptr<int>> o(std::in_place, new int(7));
  int* p = (*o).get();
  o.reset();
  return *p;
}
int reset_disengaged_ok() {
  std::optional<int> o;
  o.reset();
  if (o.has_value()) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}
int emplace_scalar_alias_ok() {
  std::optional<int> o(7);
  int* p = &*o;
  o.emplace(9);
  if (*p != 9) {
    int* q = nullptr;
    return *q;
  }
  return 0;
}
int emplace_owned_old_pointee_bad() {
  std::optional<std::unique_ptr<int>> o(std::in_place, new int(7));
  int* p = (*o).get();
  o.emplace(new int(9));
  return *p;
}
int emplace_owned_object_alias_ok() {
  std::optional<std::unique_ptr<int>> o(std::in_place, new int(7));
  auto& old_object = *o;
  o.emplace(new int(9));
  return *old_object;
}
int reset_emplace_scalar_alias_ok() {
  std::optional<int> o(7);
  int* p = &*o;
  o.reset();
  o.emplace(9);
  return *p;
}
int reset_emplace_owned_object_alias_ok() {
  std::optional<std::unique_ptr<int>> o(std::in_place, new int(7));
  auto& object = *o;
  o.reset();
  o.emplace(new int(9));
  return *object;
}
int reset_emplace_old_pointee_bad() {
  std::optional<std::unique_ptr<int>> o(std::in_place, new int(7));
  int* p = o->get();
  o.reset();
  o.emplace(new int(9));
  return *p;
}
int deleted_optional_emplace_bad() {
  auto* o = new std::optional<int>(7);
  int* p = &**o;
  o->reset();
  delete o;
  o->emplace(9);
  return *p;
}
int reset_assign_scalar_alias_ok() {
  std::optional<int> o(7);
  int* p = &*o;
  o.reset();
  o = 9;
  return *p;
}
int reset_assign_owned_object_alias_ok() {
  std::optional<std::unique_ptr<int>> o(std::in_place, new int(7));
  auto& object = *o;
  o.reset();
  o = std::make_unique<int>(9);
  return *object;
}
int reset_move_optional_owned_object_alias_ok() {
  std::optional<std::unique_ptr<int>> o(std::in_place, new int(7));
  auto& object = *o;
  std::optional<std::unique_ptr<int>> other(std::make_unique<int>(9));
  o.reset();
  o = std::move(other);
  return *object;
}
int reset_assign_old_pointee_bad() {
  std::optional<std::unique_ptr<int>> o(std::in_place, new int(7));
  int* p = o->get();
  o.reset();
  o = std::make_unique<int>(9);
  return *p;
}
int nested_empty_bad() {
  std::optional<std::optional<int>> o(std::in_place);
  return **o;
}
int nested_nullopt_bad() {
  std::optional<std::optional<int>> o(std::in_place, std::nullopt);
  return **o;
}
int nested_engaged_ok() {
  std::optional<std::optional<int>> o(std::in_place, 7);
  return **o;
}
int nested_pointer_ok() {
  std::optional<int> inner(7);
  std::optional<std::optional<int>*> outer(std::in_place, &inner);
  return ***outer;
}
} // namespace ph3_optional_regressions

#endif
