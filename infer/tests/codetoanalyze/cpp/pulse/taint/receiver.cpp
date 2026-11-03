/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

extern void* __infer_taint_source();
extern void __infer_taint_sink(void*);

namespace receiver_taint {

struct Receiver {
  void* value;

  void sanitize(void*) {}
  void sanitize_external(void*);
  void sanitize_by_position(void*) {}
  static void sanitize_argument(void*) {}

  void entry_receiver_source_bad() { __infer_taint_sink(value); }
};

void unsanitized_receiver_bad() {
  Receiver receiver{__infer_taint_source()};
  __infer_taint_sink(receiver.value);
}

void sanitized_receiver_ok() {
  Receiver receiver{__infer_taint_source()};
  receiver.sanitize(nullptr);
  __infer_taint_sink(receiver.value);
}

void sanitized_external_receiver_ok() {
  Receiver receiver{__infer_taint_source()};
  receiver.sanitize_external(nullptr);
  __infer_taint_sink(receiver.value);
}

void sanitizer_preserves_argument_bad() {
  Receiver receiver{nullptr};
  void* argument = __infer_taint_source();
  receiver.sanitize(argument);
  __infer_taint_sink(argument);
}

void receiver_argument_position_ok() {
  Receiver receiver{__infer_taint_source()};
  receiver.sanitize_by_position(nullptr);
  __infer_taint_sink(receiver.value);
}

void static_sanitizer_ok() {
  void* argument = __infer_taint_source();
  Receiver::sanitize_argument(argument);
  __infer_taint_sink(argument);
}

} // namespace receiver_taint
