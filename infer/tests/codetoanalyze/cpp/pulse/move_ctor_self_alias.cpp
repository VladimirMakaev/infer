/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
#include <new>

// A value type whose move constructor default-constructs and then move-ASSIGNS, where the move
// assignment opens with the usual self-check. The self-check gives the assignment a disjunct
// whose precondition says [this] and the source are one object; the constructor used to inherit
// it, and then every caller had to satisfy it.
struct Heavy {
  Heavy(void* arena);
  Heavy() noexcept : Heavy(nullptr) {}
  Heavy(Heavy&& from) noexcept : Heavy() { *this = static_cast<Heavy&&>(from); }
  Heavy& operator=(Heavy&& from) noexcept {
    if (this == &from) {
      return *this;
    }
    swap_(&from);
    return *this;
  }
  void swap_(Heavy* other);
  ~Heavy();
  int get() const;
};

enum class Which : unsigned char { eEmpty, eValue, eError };

// union storage, as a variant-like container holds its value
template <class V, class E>
struct Union {
  union {
    V value_;
    E error_;
  };
  Which which_ = Which::eEmpty;

  Union(const Union&) noexcept {}
  ~Union() noexcept {}
  V& value() & noexcept { return value_; }
  V&& value() && noexcept { return static_cast<V&&>(value_); }
};

template <class V, class E>
struct Storage : Union<V, E> {
  Storage(Storage&& that) noexcept : Union<V, E>(that) {
    assign(static_cast<Storage&&>(that));
  }
  ~Storage() noexcept { clear(); }
  void clear() noexcept {
    switch (this->which_) {
      case Which::eValue:
        this->value().~V();
      default:
        break;
    }
  }
  template <class... Vs>
  void assignValue(Vs&&... vs) noexcept {
    if (this->which_ == Which::eValue) {
    } else {
      ::new ((void*)&this->value()) V(static_cast<Vs&&>(vs)...);
      this->which_ = Which::eValue;
    }
  }
  template <class Other>
  void assign(Other&& that) noexcept {
    switch (that.which_) {
      case Which::eValue:
        this->assignValue(static_cast<Other&&>(that).value());
      default:
        break;
    }
  }
};

template <class V, class E>
class Holder final : Storage<V, E> {
 public:
  Holder(Holder&&) noexcept = default;
  V* operator->() noexcept { return &this->value(); }
};

using H = Holder<Heavy, int>;

H make_holder();

// a by-value parameter returned by value: it is move-constructed from the caller's temporary and
// then destroyed, so the chain below moves the holder twice
H pass_holder(H t) noexcept { return t; }

// Nothing is destroyed early here. The only object torn down is a temporary the chain has already
// moved out of, so the result is alive whether or not it is read.
//
// Pulse used to report both of these. The constructor of [Heavy] inherited a "this is the source"
// precondition from the self-check in its move assignment; satisfying it at the placement new in
// [assignValue] made the destination's and the source's union member the same address, because
// two objects being distinct says nothing about their fields; and the temporary's destructor then
// ended the lifetime of the value the caller keeps.
int move_chain_result_unused_ok() {
  auto result = pass_holder(make_holder());
  (void)result;
  return 0;
}

int move_chain_result_read_ok() {
  auto result = pass_holder(make_holder());
  return result->get();
}

// the object really is destroyed before the read
int self_alias_destruct_then_read_bad(Heavy* p) {
  p->~Heavy();
  return p->get();
}

// the same, with the explicit destructor call one frame down in an ordinary member function
struct HeavySlot {
  Heavy h_;
  void destruct() { h_.~Heavy(); }
  Heavy& get() { return h_; }
};

int self_alias_slot_destruct_then_read_bad(HeavySlot* s) {
  s->destruct();
  return s->get().get();
}

// placement new begins a new lifetime in the storage, so the read is fine
int self_alias_destruct_reconstruct_read_ok(Heavy* p) {
  p->~Heavy();
  ::new ((void*)p) Heavy();
  return p->get();
}
