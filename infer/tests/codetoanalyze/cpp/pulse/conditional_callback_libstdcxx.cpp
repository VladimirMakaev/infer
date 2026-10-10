/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Minimal std::function interface, following std_containers_libstdcxx.cpp.
// Do not include standard headers: this isolates callable specialization from
// libc++'s unresolved callable-management paths, which still retain
// conservative reports in conditional_callback.cpp. This does not cover the
// full libc++ or folly::Function implementation's construction/destruction
// machinery.
namespace std {
template <class Signature>
class function;

template <class Result, class... Args>
class function<Result(Args...)> {
 public:
  template <class Callable>
  function(Callable&& callable) {}
  ~function() {}
  Result operator()(Args...) const;

 private:
  void* target_;
};
} // namespace std

namespace conditional_callback_libstdcxx {
using Callback = std::function<int(const char*, unsigned)>;

struct Reader {
  unsigned size;
  const char* data;

  int dispatch(Callback&& callback) const {
    if (size == 0)
      return callback(nullptr, 0);
    return callback(data, size);
  }
};

int guarded_ok(const Reader& reader, char* out) {
  if (reader.size == 0)
    return 0;
  // Previously reported in dispatch's specialized summary, before rejecting
  // its size == 0 precondition against the caller's size != 0 guard.
  return reader.dispatch([out](const char* chunk, unsigned) {
    *out = *chunk;
    return 0;
  });
}

int unknown_size_bad(const Reader& reader, char* out) {
  return reader.dispatch([out](const char* chunk, unsigned) {
    *out = *chunk;
    return 0;
  });
}

int forward(const Reader& reader, Callback&& callback) {
  return reader.dispatch(static_cast<Callback&&>(callback));
}

int nested_guard_ok(const Reader& reader, char* out) {
  if (reader.size == 0)
    return 0;
  return forward(reader, [out](const char* chunk, unsigned) {
    *out = *chunk;
    return 0;
  });
}

int empty_bad(char* out) { return unknown_size_bad(Reader{0, nullptr}, out); }

int nonzero_null_bad(char* out) { return guarded_ok(Reader{1, nullptr}, out); }

int mutate_after_guard_bad(Reader& reader, char* out) {
  if (reader.size == 0)
    return 0;
  reader.size = 0;
  return reader.dispatch([out](const char* chunk, unsigned) {
    *out = *chunk;
    return 0;
  });
}
} // namespace conditional_callback_libstdcxx
