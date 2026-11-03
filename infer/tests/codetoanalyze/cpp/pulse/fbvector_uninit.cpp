/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <utility>

// The storage shape that matters here is the one fbvector.cpp's stand-in does not have: the
// pointers live in a nested `Impl` subobject and nothing gives them a default member
// initializer, exactly as in the real header. The constructor is modelled, so the real
// initialization never runs during the analysis; `reserve` is not modelled, so its real body
// does run and reads that storage. Without the constructor model recording that it
// initialized the object, every such read is a PULSE_UNINITIALIZED_VALUE.
//
// The class has to keep the name the models match on, so the stand-in here and the one in
// fbvector.cpp are two bodies for the same template. They are instantiated at different element
// types on purpose -- the suite captures the whole directory at once and rejects two source
// files that define the same procedure.

void* checked_allocate(std::size_t n);

namespace folly {

template <class T, class Allocator = std::allocator<T>>
class fbvector {
  struct Impl {
    T* b_;
    T* e_;
    T* z_;
    Impl() : b_(nullptr), e_(nullptr), z_(nullptr) {}
  };
  Impl impl_;

 public:
  using value_type = T;
  using reference = value_type&;
  using iterator = T*;
  using size_type = std::size_t;

  fbvector() = default;
  fbvector(const fbvector& other) {}
  fbvector(fbvector&& other) {}
  fbvector(std::initializer_list<T> il) {}
  ~fbvector() = default;

  iterator begin() noexcept { return impl_.b_; }
  iterator end() noexcept { return impl_.e_; }
  size_type size() const noexcept { return size_type(impl_.e_ - impl_.b_); }
  T* data() noexcept { return impl_.b_; }
  reference operator[](size_type n) { return impl_.b_[n]; }

  // Reads the capacity pointer, which is what production reported as uninitialized.
  void reserve(size_type n) {
    if (static_cast<size_type>(impl_.z_ - impl_.b_) >= n) {
      return;
    }
    T* nb = static_cast<T*>(checked_allocate(n * sizeof(T)));
    impl_.b_ = nb;
    impl_.e_ = nb;
    impl_.z_ = nb + n;
  }

  void push_back(const T& v) { reserve(size() + 1); }
};

} // namespace folly

namespace fbvector_uninit {

// The element type that keeps this file's instantiation distinct from fbvector.cpp's.
struct Elem {
  int v;
};

int sink(int x);

// The regression: an fbvector member of a default-constructed object, then reserve().
struct Holder {
  folly::fbvector<Elem> items;
  folly::fbvector<Elem> more;
};

void member_reserve_ok() {
  Holder h;
  h.items.reserve(4);
  h.more.reserve(8);
}

// Every constructor that is modelled has to record the initialization, so there is one case per
// model. They are all written in the member shape on purpose: measured against the unfixed
// build, a plain local (local_reserve_ok, copy_constructed_reserve_ok below) does NOT reproduce,
// and only a vector reached as a field through `this` does -- which is also the only shape
// production reported.
struct HolderCopy {
  folly::fbvector<Elem> items;
  explicit HolderCopy(const folly::fbvector<Elem>& src) : items(src) {}
};

void member_copy_reserve_ok(const folly::fbvector<Elem>& src) {
  HolderCopy h(src);
  h.items.reserve(4);
}

struct HolderMove {
  folly::fbvector<Elem> items;
  explicit HolderMove(folly::fbvector<Elem>&& src) : items(std::move(src)) {}
};

void member_move_reserve_ok(folly::fbvector<Elem>&& src) {
  HolderMove h(std::move(src));
  h.items.reserve(4);
}

struct HolderInitList {
  folly::fbvector<Elem> items;
  HolderInitList() : items{Elem{1}, Elem{2}} {}
};

void member_initlist_reserve_ok() {
  HolderInitList h;
  h.items.reserve(4);
}

// The two non-member shapes. Kept because they are the obvious thing to write, and recorded here
// as what they are: silent on the unfixed build too, so they hold nothing in place by themselves.
void local_reserve_ok() {
  folly::fbvector<Elem> v;
  v.reserve(4);
}

void copy_constructed_reserve_ok(const folly::fbvector<Elem>& other) {
  folly::fbvector<Elem> v{other};
  v.reserve(4);
}

void nested_member_reserve_ok() {
  struct Outer {
    Holder inner;
  };
  Outer o;
  o.inner.items.reserve(2);
}

// TP control that this change keeps: a pointer into the storage outliving the container.
Elem* escape_data() {
  folly::fbvector<Elem> v;
  v.push_back(Elem{1});
  return v.data();
}

int use_after_destruction_bad() {
  Elem* p = escape_data();
  return sink(p->v);
}

// Ownership control: a container of raw owning pointers really does leak them, and still
// reports with this change in. This change removes an Uninitialized attribute and writes
// nothing, so unlike a model that overwrites a real pointer field it cannot launder the
// allocation out of the state.
int leak_through_vector_bad() {
  folly::fbvector<Elem*> v;
  Elem* p = new Elem{1};
  v.push_back(p);
  return sink(p->v);
}

// The acknowledged cost of leaving reserve unmodelled: a reference invalidated by reserve
// reallocating is not reported. Kept as an FN_ so the loss is visible in the suite rather
// than silently absent.
int FN_reference_dangling_after_reserve() {
  folly::fbvector<Elem> v;
  v.push_back(Elem{1});
  Elem& r = v[0];
  v.reserve(1000);
  return sink(r.v);
}

} // namespace fbvector_uninit
