/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

namespace sizeof_pack {

struct context {
  int next_id;
  int next_arg_id() { return next_id++; }
};

template <typename T>
int handle(context& ctx) {
  return ctx.next_id;
}

template <typename... Args>
struct dispatcher {
  using handler = int (*)(context&);
  static constexpr int num_handlers = sizeof...(Args);
  context ctx;
  handler handlers[num_handlers > 0 ? num_handlers : 1];

  dispatcher() : ctx{0}, handlers{&handle<Args>...} {}

  int dispatch(int id) {
    return id >= 0 && id < num_handlers ? handlers[id](ctx) : -1;
  }
};

int dispatch_without_handlers_ok() {
  dispatcher<> d;
  int id = d.ctx.next_arg_id();
  return d.dispatch(id);
}

int dispatch_with_handlers_ok() {
  dispatcher<int, char> d;
  int id = d.ctx.next_arg_id();
  return d.dispatch(id);
}

template <typename... Ts>
int count(const Ts&... ts) {
  return sizeof...(Ts) + sizeof...(ts);
}

int count_two_then_null_deref_bad() {
  if (count(1, 'a') == 4) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}

int count_two_ok() {
  if (count(1, 'a') != 4) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}

} // namespace sizeof_pack
