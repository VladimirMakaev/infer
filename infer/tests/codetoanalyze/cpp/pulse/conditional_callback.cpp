/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <functional>
#include <utility>

namespace conditional_callback {
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

int with_inner_guard_ok(const Reader& reader, char* out) {
  return reader.dispatch([out](const char* chunk, unsigned n) {
    if (n == 0)
      return 0;
    *out = *chunk;
    return 0;
  });
}

int inner_guard_nonzero_null_bad(char* out) {
  return with_inner_guard_ok(Reader{1, nullptr}, out);
}

int forward(const Reader& reader, Callback&& callback) {
  return reader.dispatch(std::move(callback));
}

int nested_guard_ok(const Reader& reader, char* out) {
  if (reader.size == 0)
    return 0;
  return forward(reader, [out](const char* chunk, unsigned) {
    *out = *chunk;
    return 0;
  });
}

int wrong_reader_guard_bad(const Reader& checked,
                           const Reader& used,
                           char* out) {
  if (checked.size == 0)
    return 0;
  return used.dispatch([out](const char* chunk, unsigned) {
    *out = *chunk;
    return 0;
  });
}

Callback make_callback() {
  return [](const char* chunk, unsigned) { return *chunk; };
}

int shared_guard_ok(const Reader& reader) {
  if (reader.size == 0)
    return 0;
  return reader.dispatch(make_callback());
}

int shared_unknown_bad(const Reader& reader) {
  return reader.dispatch(make_callback());
}

extern void opaque(Callback&);
extern void opaque_void(void*);

int opaque_before_bad(const Reader& reader) {
  auto callback = make_callback();
  opaque(callback);
  if (reader.size == 0)
    return 0;
  return reader.dispatch(std::move(callback));
}

int opaque_after_bad(const Reader& reader) {
  if (reader.size == 0)
    return 0;
  auto callback = make_callback();
  int result = reader.dispatch(Callback(callback));
  opaque(callback);
  return result;
}

int opaque_erased_bad(const Reader& reader) {
  auto callback = make_callback();
  opaque_void(&callback);
  if (reader.size == 0)
    return 0;
  return reader.dispatch(std::move(callback));
}

Callback return_after_guard_bad(const Reader& reader) {
  auto callback = make_callback();
  if (reader.size != 0)
    reader.dispatch(Callback(callback));
  return callback;
}

Callback global_callback;
void store_global_bad(const Reader& reader) {
  auto callback = make_callback();
  if (reader.size == 0)
    return;
  reader.dispatch(Callback(callback));
  global_callback = callback;
}

int mutate_after_guard_bad(Reader& reader) {
  if (reader.size == 0)
    return 0;
  reader.size = 0;
  return reader.dispatch(make_callback());
}

} // namespace conditional_callback
