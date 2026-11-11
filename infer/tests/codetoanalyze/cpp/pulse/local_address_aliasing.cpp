/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <memory>
#include <utility>

namespace local_address_aliasing {

struct Item {
  int x;
};

// the self-assignment case of the model cannot happen: [p] is a local variable
// and [other] exists before it, so resetting [other] does not empty [p]
int shared_ptr_reset_source_after_assign_to_local_ok(
    std::shared_ptr<Item>& other) {
  std::shared_ptr<Item> p = std::make_shared<Item>();
  p = other;
  if (p == nullptr) {
    return 0;
  }
  other.reset();
  return p->x;
}

int shared_ptr_assign_to_local_then_reset_bad(
    const std::shared_ptr<Item>& other) {
  std::shared_ptr<Item> p;
  p = other;
  p.reset();
  return p->x;
}

// both sides are parameters and may be the same object, which reset() empties
int shared_ptr_self_assign_param_bad(std::shared_ptr<Item>& dst,
                                     std::shared_ptr<Item>& src) {
  dst.reset();
  dst = src;
  return dst->x;
}

int call_shared_ptr_self_assign_param_bad() {
  std::shared_ptr<Item> p = std::make_shared<Item>();
  return shared_ptr_self_assign_param_bad(p, p);
}

struct Counter {
  int n;
  Counter() : n(0) {}
  Counter& operator=(const Counter& other) {
    if (this != &other) {
      n = other.n;
    }
    return *this;
  }
};

int read_copy(const Counter* c) {
  Counter local;
  local = *c;
  return local.n;
}

int copy_assign_to_local_ok(Counter* c) {
  c->n = 5;
  if (read_copy(c) != 5) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}

int copy_assign_to_local_twice_ok(const Counter* c) {
  return read_copy(c) + read_copy(c);
}

// the condition [this != &other] of the copy assignment leaves a disequality
// between [c] and the address of the local of [read_copy] in its summary, which
// makes the issue latent
int FN_copy_assign_to_local_bad(Counter* c) {
  c->n = 5;
  if (read_copy(c) == 5) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}

struct Buffer {
  int* data;
  Buffer() : data(nullptr) {}
  ~Buffer() { delete data; }
  Buffer& operator=(Buffer&& other) {
    if (this != &other) {
      delete data;
      data = other.data;
      other.data = nullptr;
    }
    return *this;
  }
};

struct Holder {
  Buffer buffer;
};

void unknown_update(Holder* holder);

// [holder->buffer] is a fresh value after the unknown call, which cannot be the
// address of [tmp] either
Buffer* move_through_local_after_unknown_call_ok(Holder* holder) {
  Buffer tmp;
  int* data = holder->buffer.data;
  unknown_update(holder);
  tmp = std::move(holder->buffer);
  holder->buffer = std::move(tmp);
  return &holder->buffer;
}

// an element of a raw buffer: its address is pointer arithmetic over the buffer,
// not a field of a known object
struct RawVec {
  void* buf;
  std::shared_ptr<Item>* at(unsigned i) {
    return static_cast<std::shared_ptr<Item>*>(buf) + i;
  }
};

void move_into_elem(RawVec* v, unsigned i, std::shared_ptr<Item>&& p) {
  *v->at(i) = std::move(p);
}

void copy_into_elem(RawVec* v, unsigned i, const std::shared_ptr<Item>& p) {
  *v->at(i) = p;
}

// the temporary is moved into the element, then the element is returned: the
// element is not the temporary
std::shared_ptr<Item>& move_temporary_into_elem_then_return_ok(RawVec* v,
                                                               unsigned i) {
  move_into_elem(v, i, std::make_shared<Item>());
  return *v->at(i);
}

std::shared_ptr<Item>& copy_temporary_into_elem_then_return_ok(RawVec* v,
                                                               unsigned i) {
  copy_into_elem(v, i, std::make_shared<Item>());
  return *v->at(i);
}

// the same two frames, but the source is a named local instead of a temporary
std::shared_ptr<Item>& move_local_into_elem_then_return_ok(RawVec* v,
                                                           unsigned i) {
  std::shared_ptr<Item> local = std::make_shared<Item>();
  move_into_elem(v, i, std::move(local));
  return *v->at(i);
}

// must-report twin: here the reference really does name the temporary
const std::shared_ptr<Item>& return_reference_to_temporary_bad() {
  const std::shared_ptr<Item>& r = std::make_shared<Item>();
  return r;
}

// two parameters that the caller aliases: assigning one to the other must stay
// harmless, the pointee is still owned afterwards
void copy_assign_params(std::shared_ptr<Item>& a, std::shared_ptr<Item>& b) {
  a = b;
}

int call_copy_assign_aliased_params_ok() {
  std::shared_ptr<Item> p = std::make_shared<Item>();
  copy_assign_params(p, p);
  return p->x;
}

void move_assign_params(std::shared_ptr<Item>& a, std::shared_ptr<Item>& b) {
  a = std::move(b);
}

int call_move_assign_aliased_params_ok() {
  std::shared_ptr<Item> p = std::make_shared<Item>();
  move_assign_params(p, p);
  return p->x;
}

// Both sides are elements of the same buffer, so the two addresses are alike: when the
// caller supplies the same index they really are the same object, the assignment is a
// self-assignment, and the dereference after the reset is a null one. The self disjunct
// must survive here.
int rawvec_self_assign_bad(RawVec* v, unsigned i, unsigned j) {
  v->at(i)->reset();
  *v->at(i) = *v->at(j);
  return (*v->at(i))->x;
}

int call_rawvec_same_index_bad(RawVec* v) {
  return rawvec_self_assign_bad(v, 0, 0);
}

int rawvec_move_self_assign_bad(RawVec* v, unsigned i, unsigned j) {
  v->at(i)->reset();
  *v->at(i) = std::move(*v->at(j));
  return (*v->at(i))->x;
}

int call_rawvec_move_same_index_bad(RawVec* v) {
  return rawvec_move_self_assign_bad(v, 0, 0);
}

void sink_pointer(void* p);

// The same self-assignment as above with one unrelated line that computes an address FROM
// dst. That puts the address of dst on the right-hand side of a linear equation without
// making dst itself a computed address, and the test for "is this operand computed" must
// not be fooled by it: the caller still passes the same object twice, so the dereference
// after the reset is still a null one.
int range_only_self_assign_bad(std::shared_ptr<Item>& dst,
                               std::shared_ptr<Item>& src,
                               unsigned n) {
  sink_pointer(&dst + n);
  dst.reset();
  dst = src;
  return dst->x;
}

int call_range_only_aliased_bad(unsigned n) {
  std::shared_ptr<Item> p = std::make_shared<Item>();
  return range_only_self_assign_bad(p, p, n);
}

// A round trip that adds an offset and takes it away again leaves the address where it
// started, so these two really are the same object and the dereference after the reset is
// a null one. The arithmetic must not make the test for "is this operand computed" treat
// the two sides differently.
int roundtrip_self_assign_bad(std::shared_ptr<Item>* a,
                              std::shared_ptr<Item>& b,
                              unsigned n) {
  std::shared_ptr<Item>* p = (a + n) - n;
  p->reset();
  *p = b;
  return (*p)->x;
}

int call_roundtrip_aliased_bad(std::shared_ptr<Item>* a, unsigned n) {
  return roundtrip_self_assign_bad(a, *a, n);
}

// A type whose assignment operator is written out rather than modelled, and which
// guards against self-assignment the way such operators usually do. The guard makes
// the analysis consider the branch where the two sides are the same object, which is
// how a temporary's address reaches an element of a raw buffer.
struct Guarded {
  int* p;
  Guarded() : p(nullptr) {}
  explicit Guarded(int* q) : p(q) {}
  Guarded(const Guarded& o) : p(o.p) {}
  Guarded& operator=(const Guarded& o) {
    if (this != &o) {
      p = o.p;
    }
    return *this;
  }
  Guarded& operator=(Guarded&& o) {
    if (this != &o) {
      p = o.p;
      o.p = nullptr;
    }
    return *this;
  }
};

struct GuardedVec {
  void* buf;
  Guarded* at(unsigned i) { return static_cast<Guarded*>(buf) + i; }
};

void move_guarded_into_elem(GuardedVec* v, unsigned i, Guarded&& g) {
  *v->at(i) = std::move(g);
}

// the element was computed from the buffer, so it is not the temporary that was
// moved into it, however the two came to be equated
Guarded& move_guarded_temporary_into_elem_then_return_ok(GuardedVec* v,
                                                         unsigned i,
                                                         int* q) {
  move_guarded_into_elem(v, i, Guarded(q));
  return *v->at(i);
}

// the same, with the temporary forwarded through one more frame before it lands
void forward_guarded_into_elem(GuardedVec* v, unsigned i, Guarded&& g) {
  move_guarded_into_elem(v, i, std::move(g));
}

Guarded& move_guarded_temporary_two_frames_then_return_ok(GuardedVec* v,
                                                          unsigned i,
                                                          int* q) {
  forward_guarded_into_elem(v, i, Guarded(q));
  return *v->at(i);
}

// The same storage pattern with the buffer owned by the returning frame: the
// element dies with the call, so this really does escape, and the exclusion
// silences it. Reported before the exclusion, not after. Two other ways of
// writing the same escape, with the arithmetic done in this frame rather than in
// at(), are not reported either way, so what is lost is this route and not
// elements of local buffers in general.
Guarded& FN_move_guarded_temporary_into_local_elem_bad(unsigned i, int* q) {
  Guarded local_buf[4];
  GuardedVec v;
  v.buf = local_buf;
  move_guarded_into_elem(&v, i, Guarded(q));
  return *v.at(i);
}

struct Elem {
  int x;
};

// Must-report twins for the escape check: a round trip adds an offset and takes it
// away again, so the address returned is the temporary's own and the escape is real.
const Elem& return_temporary_through_roundtrip_bad(long n) {
  Elem&& r = Elem();
  const Elem* q = (&r + n) - n;
  return *q;
}

const Elem* roundtrip_pointer(const Elem* p, long n) { return (p + n) - n; }

const Elem& return_temporary_through_callee_roundtrip_bad(long n) {
  Elem&& r = Elem();
  return *roundtrip_pointer(&r, n);
}

const Elem& return_temporary_plus_zero_bad() {
  Elem&& r = Elem();
  const Elem* q = &r + 0;
  return *q;
}

// The address returned here is an offset of the temporary rather than the temporary
// itself, and nothing reports it: the escape check only fires on the address the
// temporary was marked at. Recorded so that closing that gap is noticed here, because
// it would also be the first shape where declining a computed address could cost a
// real report.
const Elem* FN_return_offset_of_temporary_bad() {
  Elem&& r = Elem();
  return &r + 1;
}

} // namespace local_address_aliasing
