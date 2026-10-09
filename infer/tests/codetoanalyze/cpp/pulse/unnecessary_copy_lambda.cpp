/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <string>

namespace lambda_copies {

struct Result {
  std::string text;
};

std::string make_text();
void observe(const std::string&);

void ordinary_capture_ok() {
  std::string text = make_text();
  // The captured string is const in operator(): std::move(text) still copies.
  auto query = [text] { return Result{text}; };
}

#if __cplusplus >= 201402L
void init_capture_ok() {
  auto query = [text = make_text()] { return Result{text}; };
}
#endif

void mutable_capture_bad() {
  std::string text = make_text();
  auto query = [text]() mutable { return Result{text}; };
}

#if __cplusplus >= 201402L
void mutable_init_capture_bad() {
  auto query = [text = make_text()]() mutable { return Result{text}; };
}
#endif

void reference_capture_bad(std::string& text) {
  // A const call operator does not make a by-reference capture const.
  auto query = [&text] { return Result{text}; };
}

void pointer_capture_bad(std::string* text) {
  // The captured pointer is const; the string it points to remains mutable.
  auto query = [text] { return Result{*text}; };
}

void local_copy_of_capture_bad() {
  std::string text = make_text();
  // Binding the local to a const reference would avoid this copy.
  auto query = [text] {
    std::string local = text;
    observe(local);
  };
}

void local_copy_of_pointee_bad(std::string* text) {
  auto query = [text] {
    std::string local = *text;
    observe(local);
  };
}

struct Holder {
  std::string text;
};

void field_of_capture_ok() {
  Holder holder{make_text()};
  auto query = [holder] { return Result{holder.text}; };
}

struct MutableHolder {
  mutable std::string text;
};

void mutable_field_of_capture_bad() {
  MutableHolder holder{make_text()};
  // A mutable member can be moved from even when the enclosing capture is const.
  auto query = [holder] { return Result{holder.text}; };
}

void repeated_calls_ok() {
  std::string text = make_text();
  auto query = [text] { return Result{text}; };
  Result first = query();
  Result second = query();
  observe(first.text);
  observe(second.text);
}

} // namespace lambda_copies
