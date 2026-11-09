/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// An owner holds its part and the part points back at its owner, so the two
// objects are each reachable from the other. Each method then names the other's
// lock through a field, and the two identifications are mutually inverse. That
// is not a reason to stay quiet: it is the ordinary shape of a real deadlock
// between a parent and its child, and the only reason to doubt it is a lock the
// two might share, which is what OwnedWithGate below has and these do not.

#include <memory>
#include <mutex>

namespace owner_backpointer {

class Owned;

// owns the part through a pointer; each locks its own mutex and then the other's
class OwnerByPointer {
 public:
  void lock_self_then_part_bad();

  std::mutex m_;
  Owned* part_;
};

class Owned {
 public:
  void lock_self_then_owner_bad() {
    std::lock_guard<std::mutex> mine(m_);
    std::lock_guard<std::mutex> theirs(owner_->m_);
  }

  OwnerByPointer* owner_;
  std::mutex m_;
};

void OwnerByPointer::lock_self_then_part_bad() {
  std::lock_guard<std::mutex> mine(m_);
  std::lock_guard<std::mutex> theirs(part_->m_);
}

// owns the part through a smart pointer, as the reported real case does
class HeldChild;

class HeldParent {
 public:
  void lock_self_then_child_bad();

  std::mutex m_;
  std::unique_ptr<HeldChild> child_;
};

class HeldChild {
 public:
  void lock_self_then_parent_bad() {
    std::lock_guard<std::mutex> mine(m_);
    std::lock_guard<std::mutex> theirs(parent_->m_);
  }

  HeldParent* parent_;
  std::mutex m_;
};

void HeldParent::lock_self_then_child_bad() {
  std::lock_guard<std::mutex> mine(m_);
  std::lock_guard<std::mutex> theirs(child_->m_);
}

// THE CONTROL. The same cycle, but both methods first take a lock named by one
// field of one class. The two paths to it cannot be identified, so it cannot be
// shown to be one object -- and it cannot be shown to be two either, so a gate
// serialising the two methods is possible and nothing is reported.
class GateOwned;

class GateHolder {
 public:
  std::mutex gate_;
};

class GateOwner {
 public:
  void gate_then_self_then_owned_ok();

  GateHolder* holder_;
  std::mutex m_;
  GateOwned* owned_;
};

class GateOwned {
 public:
  void gate_then_self_then_owner_ok() {
    std::lock_guard<std::mutex> gate(holder_->gate_);
    std::lock_guard<std::mutex> mine(m_);
    std::lock_guard<std::mutex> theirs(owner_->m_);
  }

  GateHolder* holder_;
  GateOwner* owner_;
  std::mutex m_;
};

void GateOwner::gate_then_self_then_owned_ok() {
  std::lock_guard<std::mutex> gate(holder_->gate_);
  std::lock_guard<std::mutex> mine(m_);
  std::lock_guard<std::mutex> theirs(owned_->m_);
}

} // namespace owner_backpointer
