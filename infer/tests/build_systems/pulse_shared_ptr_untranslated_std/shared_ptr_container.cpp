#include <memory>
#include <utility>
#include <vector>

// Insertions into a std::vector of std::shared_ptr whose element constructor the container
// model cannot find, which is when it falls back to applying the smart pointer model itself.
// See the Makefile for which insertion shapes reach that fallback and which do not: the cases
// here are only the ones that do, and each was checked to behave differently on an Infer
// without the fix it guards.

namespace shared_ptr_container {

struct Payload {
  int value;
};

struct NoDelete {
  void operator()(Payload*) const {}
};

// A std::unique_ptr whose deleter frees nothing is not a std::shared_ptr, so the shared
// pointer's ownership semantics must not be applied to it: nothing here ever frees the object
// and reading it after the container is cleared is correct. Giving this source the shared
// pointer's model instead reads a reference count it never had and drops the deleter, which
// reports the read as a use after delete.
int custom_deleter_keeps_object_ok(Payload* raw) {
  std::unique_ptr<Payload, NoDelete> held(raw);
  std::vector<std::shared_ptr<Payload>> owner;
  owner.emplace_back(std::move(held));
  owner.clear();
  return raw->value;
}

void no_delete(Payload*) {}

// The same with a function pointer deleter, which is a different type shape.
int function_deleter_keeps_object_ok(Payload* raw) {
  std::unique_ptr<Payload, void (*)(Payload*)> held(raw, &no_delete);
  std::vector<std::shared_ptr<Payload>> owner;
  owner.emplace_back(std::move(held));
  owner.clear();
  return raw->value;
}

// Control: no container is involved, so the reset really does drop the last reference and this
// has to stay reported however an insertion behaves.
int sole_owner_dropped_bad(std::shared_ptr<Payload>& held) {
  if (!held) {
    return 0;
  }
  Payload* raw = held.get();
  held.reset();
  return raw->value;
}

// Control for the opposite failure. A fix here adds a reference; it must not silence the
// checker. The container is emptied as well, so nothing owns the object and this is still a
// genuine use after delete.
int container_dropped_last_owner_bad(std::shared_ptr<Payload>& held) {
  if (!held) {
    return 0;
  }
  Payload* raw = held.get();
  std::vector<std::shared_ptr<Payload>> owner;
  owner.emplace_back(held);
  held.reset();
  owner.clear();
  return raw->value;
}

// A known false positive, and not one this fallback causes: it is reported identically with and
// without every fix above. A moved-from source owns nothing, so its count cannot reach two and
// the dereference below is unreachable. Left in place, and marked, so that a later reader does
// not attribute it to the insertion path.
int FP_moved_source_is_empty_ok(std::shared_ptr<Payload>& held) {
  if (!held || held.use_count() != 1) {
    return 0;
  }
  std::vector<std::shared_ptr<Payload>> owner;
  owner.push_back(std::move(held));
  if (held.use_count() == 2) {
    Payload* impossible = nullptr;
    return impossible->value;
  }
  return 0;
}

} // namespace shared_ptr_container
