/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <string>
#include <utility>

namespace late_capture {

std::string make_text();
void consume(std::string);
void observe(const std::string&);
void opaque_callback(void*);
void opaque_integer(int*);

void created_after_called_ok() {
  std::string text = make_text();
  consume(text);
  auto query = [&] { observe(text); };
  query();
}

void created_before_called_after_ok() {
  std::string text = make_text();
  auto query = [&] { observe(text); };
  consume(text);
  query();
}

void created_after_never_called_bad() {
  std::string text = make_text();
  consume(text);
  auto query = [&] { observe(text); };
}

void created_before_never_called_bad() {
  std::string text = make_text();
  auto query = [&] { observe(text); };
  consume(text);
}

void called_only_before_bad() {
  std::string text = make_text();
  auto query = [&] { observe(text); };
  query();
  consume(text);
}

void reference_alias_called_after_ok() {
  std::string text = make_text();
  auto query = [&] { observe(text); };
  auto& alias = query;
  consume(text);
  alias();
}

void reference_alias_called_only_before_bad() {
  std::string text = make_text();
  auto query = [&] { observe(text); };
  auto& alias = query;
  alias();
  consume(text);
}

void copied_closure_called_after_ok() {
  std::string text = make_text();
  auto query = [&] { observe(text); };
  auto copy = query;
  consume(text);
  copy();
}

void copied_closure_never_called_bad() {
  std::string text = make_text();
  auto query = [&] { observe(text); };
  auto copy = query;
  consume(text);
}

void copied_closure_called_only_before_bad() {
  std::string text = make_text();
  auto query = [&] { observe(text); };
  auto copy = query;
  copy();
  consume(text);
}

void moved_closure_called_after_ok() {
  std::string text = make_text();
  auto query = [&] { observe(text); };
  auto moved = std::move(query);
  consume(text);
  moved();
}

void moved_closure_never_called_bad() {
  std::string text = make_text();
  auto query = [&] { observe(text); };
  auto moved = std::move(query);
  consume(text);
}

void pointer_alias_called_after_ok() {
  std::string text = make_text();
  auto query = [&] { observe(text); };
  auto* pointer = &query;
  consume(text);
  (*pointer)();
}

void source_reference_alias_ok() {
  std::string text = make_text();
  std::string& alias = text;
  auto query = [&alias] { observe(alias); };
  consume(text);
  query();
}

void inline_closure_called_after_ok() {
  std::string text = make_text();
  consume(text);
  [&] { observe(text); }();
}

void unrelated_closure_called_after_bad() {
  std::string text = make_text();
  std::string other = make_text();
  auto query = [&] { observe(text); };
  auto unrelated = [&] { observe(other); };
  query();
  consume(text);
  unrelated();
}

void only_second_copy_bad() {
  std::string text = make_text();
  auto query = [&] { observe(text); };
  consume(text); // The later query requires this copy.
  query();
  consume(text); // This final use can move.
}

void opaque_after_copy_ok() {
  std::string text = make_text();
  consume(text);
  auto query = [&] { observe(text); };
  opaque_callback(&query);
}

void opaque_before_copy_ok() {
  std::string text = make_text();
  auto query = [&] { observe(text); };
  opaque_callback(&query); // May retain the callback and invoke it later.
  consume(text);
}

template <typename F>
void invoke_now(const F& query) {
  query();
}

void known_helper_called_only_before_bad() {
  std::string text = make_text();
  auto query = [&] { observe(text); };
  invoke_now(query);
  consume(text);
}

template <typename F>
void retain_in_helper(F& query) {
  opaque_callback(&query);
}

void helper_escapes_before_copy_ok() {
  std::string text = make_text();
  auto query = [&] { observe(text); };
  retain_in_helper(query);
  consume(text);
}

template <typename F>
void invoke_and_modify_integer(const F& query, int* value) {
  query();
  opaque_integer(value);
}

void unrelated_unknown_effect_bad() {
  std::string text = make_text();
  int integer = 0;
  auto query = [&] { observe(text); };
  invoke_and_modify_integer(query, &integer);
  consume(text);
}

template <typename F>
struct CallbackBox {
  F query;
};

void local_field_called_after_ok() {
  std::string text = make_text();
  auto query = [&] { observe(text); };
  CallbackBox<decltype(query)> box{query};
  consume(text);
  box.query();
}

void local_field_never_called_bad() {
  std::string text = make_text();
  auto query = [&] { observe(text); };
  CallbackBox<decltype(query)> box{query};
  consume(text);
}

#if __cplusplus >= 201402L
auto make_query(std::string& text) {
  return [&text] { observe(text); };
}

void pointer_reassigned_bad() {
  std::string text = make_text();
  std::string other = make_text();
  auto query = make_query(text);
  auto unrelated = make_query(other);
  auto* pointer = &query;
  pointer = &unrelated;
  consume(text);
  (*pointer)();
}
#endif

void last_use_bad() {
  std::string text = make_text();
  consume(text);
}

void moved_then_copied_closure_called_after_ok() {
  std::string text = make_text();
  auto query = [&] { observe(text); };
  auto moved = std::move(query);
  auto copy = moved;
  consume(text);
  copy();
}

void opaque_retained_pointee_ok() {
  std::string text = make_text();
  std::string other = make_text();
  auto* pointer = &text;
  auto query = [&pointer] { observe(*pointer); };
  opaque_callback(&query); // May save the original pointee for a later read.
  pointer = &other;
  consume(text);
}

template <typename F>
void pass_to_opaque(F query) {
  opaque_callback(&query);
}

void temporary_callback_escapes_ok() {
  std::string text = make_text();
  consume(text);
  pass_to_opaque([&] { observe(text); });
}

void discarded_closure_after_copy_bad() {
  std::string text = make_text();
  auto query = [&] { observe(text); };
  consume(text);
  (void)query;
}

void discarded_pointer_after_copy_bad() {
  std::string text = make_text();
  auto query = [&] { observe(text); };
  consume(text);
  auto* pointer = &query;
  (void)pointer;
}

void by_value_capture_called_after_bad() {
  std::string text = make_text();
  auto query = [text] { observe(text); };
  consume(text);
  query();
}

void external_store_after_copy_ok(void** destination) {
  std::string text = make_text();
  auto query = [&] { observe(text); };
  consume(text);
  *destination = &query;
}

} // namespace late_capture
