/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#if __cplusplus >= 201703L
#include <optional>
#include <string>
namespace optional_field_alias {
struct Record {
  std::string label;
  std::string other;
  int value;
};
Record make_record();
std::string make_string();
void publish(const std::string&, std::optional<std::string>);
void publish_ptr(const std::string*, std::optional<std::string>);
void publish_int(int, std::optional<std::string>);
void publish_opt(std::optional<std::string>);
void publish_two(std::optional<std::string>, std::optional<std::string>);
void publish_three(const std::string&,
                   std::optional<std::string>,
                   std::optional<std::string>);
void read_label(const std::string&);
void known_publish(const std::string& source, std::optional<std::string> copy) {
  if (source.empty())
    copy.reset();
}
// Moving from label while evaluating another argument could change what the
// consumer observes through its reference, regardless of argument order.
void field_alias_ok() {
  Record rec = make_record();
  publish(rec.label, std::optional<std::string>{rec.label});
}
void local_alias_ok() {
  std::string text = make_string();
  publish(text, std::optional<std::string>{text});
}
void field_known_alias_ok() {
  Record rec = make_record();
  known_publish(rec.label, std::optional<std::string>{rec.label});
}
void reference_alias_ok() {
  Record rec = make_record();
  const std::string& alias = rec.label;
  publish(alias, std::optional<std::string>{rec.label});
}
void pointer_alias_ok() {
  Record rec = make_record();
  const std::string* alias = &rec.label;
  publish_ptr(alias, std::optional<std::string>{rec.label});
}
void record_reference_alias_ok() {
  Record rec = make_record();
  Record& alias = rec;
  publish(alias.label, std::optional<std::string>{rec.label});
}
void record_pointer_alias_ok() {
  Record rec = make_record();
  Record* alias = &rec;
  publish(rec.label, std::optional<std::string>{alias->label});
}
void known_record_ok() {
  Record rec{"first", "second", 7};
  publish(rec.label, std::optional<std::string>{rec.label});
}
void canonical_pointer_ok(bool cond) {
  Record rec{"first", "a much longer label", 7};
  const std::string* p = cond ? &rec.label : &rec.other;
  if (p == &rec.label)
    publish_ptr(p, std::optional<std::string>{rec.label});
}
// Passing a distinct field does not use the copied field.
void scalar_unrelated_bad() {
  Record rec = make_record();
  publish_int(rec.value, std::optional<std::string>{rec.label});
}
void field_unrelated_bad() {
  Record rec = make_record();
  publish(rec.other, std::optional<std::string>{rec.label});
}
void unaliased_bad() {
  Record rec = make_record();
  publish_opt(std::optional<std::string>{rec.label});
}
void reassigned_pointer_bad() {
  Record rec = make_record();
  const std::string* p = &rec.label;
  p = &rec.other;
  publish_ptr(p, std::optional<std::string>{rec.label});
}
void same_source_twice_ok() {
  Record rec = make_record();
  publish_two(std::optional<std::string>{rec.label},
              std::optional<std::string>{rec.label});
}
// These copies have distinct source object addresses within the same Record.
void different_sources_twice_bad() {
  Record rec = make_record();
  publish_two(std::optional<std::string>{rec.label},
              std::optional<std::string>{rec.other});
}
void mixed_sources_bad() {
  Record rec = make_record();
  publish_three(rec.label,
                std::optional<std::string>{rec.label},
                std::optional<std::string>{rec.other});
}
void before_alias_bad() {
  Record rec = make_record();
  read_label(rec.label);
  publish_opt(std::optional<std::string>{rec.label});
}
void before_and_unrelated_after_bad() {
  Record rec = make_record();
  read_label(rec.label);
  publish_opt(std::optional<std::string>{rec.label});
  read_label(rec.other);
}
struct Nested {
  Record inner;
};
Nested make_nested();
void nested_field_ok() {
  Nested rec = make_nested();
  publish(rec.inner.label, std::optional<std::string>{rec.inner.label});
}
void reference_source_same_call_ok() {
  Record rec = make_record();
  const std::string& alias = rec.label;
  publish(rec.label, std::optional<std::string>{alias});
}
void pointer_source_same_call_ok() {
  Record rec = make_record();
  const std::string* alias = &rec.label;
  publish(rec.label, std::optional<std::string>{*alias});
}
void before_reference_alias_bad() {
  Record rec = make_record();
  const std::string& alias = rec.label;
  read_label(alias);
  publish(rec.other, std::optional<std::string>{rec.label});
}
void before_pointer_alias_bad() {
  Record rec = make_record();
  const std::string* alias = &rec.label;
  read_label(*alias);
  publish_int(rec.value, std::optional<std::string>{rec.label});
}
void two_separate_copies_bad() {
  Record rec = make_record();
  publish_opt(std::optional<std::string>{rec.label});
  publish_opt(std::optional<std::string>{rec.other});
}

} // namespace optional_field_alias

#endif
