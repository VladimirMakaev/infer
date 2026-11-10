/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <string>

struct StringBox {
  std::string text;

  explicit StringBox(const char* source) { text.append(source); }
};

const StringBox global_box("example");

std::string copy_initialized_global_ok() { return global_box.text; }

char read_initialized_global_ok() { return *global_box.text.c_str(); }

void append_more(std::string& text) { text.append(1000, 'x'); }

char read_after_append_in_callee_ok(std::string& text) {
  append_more(text);
  return *text.c_str();
}

char old_buffer_after_append_in_callee_bad(std::string& text) {
  const char* buffer = text.c_str();
  append_more(text);
  return *buffer;
}

std::string mutable_global("example");

char old_global_buffer_after_append_bad() {
  const char* buffer = mutable_global.c_str();
  append_more(mutable_global);
  return *buffer;
}

char copied_global_buffer_after_append_bad() {
  std::string copy = global_box.text;
  const char* buffer = copy.c_str();
  append_more(copy);
  return *buffer;
}
