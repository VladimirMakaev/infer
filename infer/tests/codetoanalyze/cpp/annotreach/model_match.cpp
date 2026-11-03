/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

namespace other {
struct Sink {
  void sink() {}
};
} // namespace other

namespace modeled {
void free_sink() {}

struct Sink {
  void sink() {}
  void legacy_sink() {}
  void blocked_legacy() {}
  void blocked_natural() {}
};

template <class T>
struct TemplateSink {
  void sink() {}
};

void intermediate(Sink& value) { value.sink(); }

struct NaturalSanitizer {
  Sink& value;
  NaturalSanitizer(Sink& input) : value(input) { value.sink(); }
  ~NaturalSanitizer() { value.sink(); }
};

struct LegacySanitizer {
  Sink& value;
  LegacySanitizer(Sink& input) : value(input) { value.legacy_sink(); }
  ~LegacySanitizer() { value.legacy_sink(); }
};

struct Source {
  void natural_method_bad(Sink& value) { value.sink(); }
  void legacy_method_bad(Sink& value) { value.legacy_sink(); }
  void indirect_bad(Sink& value) { intermediate(value); }
  void template_bad(TemplateSink<int>& value) { value.sink(); }
  void blocked_legacy_ok(Sink& value) { value.blocked_legacy(); }
  void blocked_natural_ok(Sink& value) { value.blocked_natural(); }
  void natural_sanitizer_ok(Sink& value) { NaturalSanitizer guard(value); }
  void legacy_sanitizer_ok(Sink& value) { LegacySanitizer guard(value); }
  void namespace_ok(other::Sink& value) { value.sink(); }
};

struct LegacySource {
  void control_bad(Sink& value) { value.legacy_sink(); }
};

void source_control_bad() { free_sink(); }
} // namespace modeled
