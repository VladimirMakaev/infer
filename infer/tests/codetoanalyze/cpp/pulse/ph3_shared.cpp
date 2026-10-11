/* Copyright (c) Meta Platforms, Inc. and affiliates.
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree. */
#include <memory>
#include <utility>

namespace ph3_shared_regressions {
int typed_null_owns_ok() {
  std::shared_ptr<int> a(static_cast<int*>(nullptr));
  std::shared_ptr<int> b(a);
  if (a.use_count() != 2 || b.use_count() != 2) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}
int nullptr_is_empty_ok() {
  std::shared_ptr<int> a(nullptr);
  if (a.use_count() != 0 || a.get() != nullptr) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}
int typed_null_count_bad() {
  std::shared_ptr<int> a(static_cast<int*>(nullptr));
  if (a.use_count() == 1) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}
int empty_alias_copy_ok() {
  int value = 7;
  std::shared_ptr<int> empty;
  std::shared_ptr<int> alias(empty, &value);
  std::shared_ptr<int> copy(alias);
  if (alias.get() != &value || copy.get() != &value || copy.use_count() != 0) {
    int* p = nullptr;
    return *p;
  }
  copy.reset();
  alias.reset();
  return value;
}
int owning_null_alias_ok() {
  std::shared_ptr<int> owner(new int(7));
  int* p = owner.get();
  std::shared_ptr<int> alias(owner, nullptr);
  if (alias.get() != nullptr || alias.use_count() != 2) {
    int* q = nullptr;
    return *q;
  }
  owner.reset();
  return *p;
}
int owning_null_alias_last_reset_bad() {
  std::shared_ptr<int> owner(new int(7));
  int* p = owner.get();
  std::shared_ptr<int> alias(owner, nullptr);
  owner.reset();
  alias.reset();
  return *p;
}
int external_alias_stays_ok() {
  int value = 8;
  std::shared_ptr<int> owner(new int(7));
  std::shared_ptr<int> alias(owner, &value);
  owner.reset();
  alias.reset();
  return value;
}
int external_alias_disposes_owner_bad() {
  int value = 8;
  std::shared_ptr<int> owner(new int(7));
  int* p = owner.get();
  std::shared_ptr<int> alias(owner, &value);
  owner.reset();
  alias.reset();
  return *p;
}
int move_disposes_destination_bad() {
  std::shared_ptr<int> source(new int(7)), destination(new int(8));
  int* old = destination.get();
  destination = std::move(source);
  return *old;
}
int move_preserves_source_pointee_ok() {
  std::shared_ptr<int> source(new int(7)), destination(new int(8));
  int* p = source.get();
  destination = std::move(source);
  if (source.use_count() != 0 || destination.use_count() != 1) {
    int* q = nullptr;
    return *q;
  }
  return *p;
}
int self_move_ok() {
  std::shared_ptr<int> a(new int(7));
  int* p = a.get();
  a = std::move(a);
  return *p;
}
int weak_owning_null_ok() {
  std::shared_ptr<int> a(static_cast<int*>(nullptr));
  std::weak_ptr<int> w(a);
  auto locked = w.lock();
  if (w.expired() || locked.use_count() != 2 || locked.get() != nullptr) {
    int* p = nullptr;
    return *p;
  }
  locked.reset();
  a.reset();
  if (!w.expired()) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}
int weak_empty_nonnull_alias_ok() {
  int value = 7;
  std::shared_ptr<int> empty;
  std::shared_ptr<int> alias(empty, &value);
  std::weak_ptr<int> w(alias);
  auto locked = w.lock();
  if (!w.expired() || locked.get() != nullptr || locked.use_count() != 0) {
    int* p = nullptr;
    return *p;
  }
  return value;
}
struct OwningNode {
  int value = 0;
  int* pointee = new int(7);
  ~OwningNode() { delete pointee; }
};
int member_alias_last_owner_bad() {
  std::shared_ptr<OwningNode> owner(new OwningNode);
  int* p = owner->pointee;
  std::shared_ptr<int> alias(owner, &owner->value);
  owner.reset();
  alias.reset();
  return *p;
}
int make_shared_alias_last_owner_bad() {
  auto owner = std::make_shared<int>(7);
  int* p = owner.get();
  std::shared_ptr<int> alias(owner, nullptr);
  owner.reset();
  alias.reset();
  return *p;
}
} // namespace ph3_shared_regressions
