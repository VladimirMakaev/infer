/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <memory>

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

} // namespace local_address_aliasing
