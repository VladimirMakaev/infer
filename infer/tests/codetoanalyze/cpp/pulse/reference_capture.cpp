/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <memory>
#include <utility>

namespace reference_capture {

void reset_shared(std::shared_ptr<int>&& pointer) {
  auto&& reset = [&pointer] { pointer.reset(); };
  reset();
}

void reset_unique(std::unique_ptr<int>&& pointer) {
  auto&& reset = [&pointer] { pointer.reset(); };
  reset();
}

void reset_nested(std::shared_ptr<int>&& pointer) {
  auto&& outer = [&pointer] {
    auto&& inner = [&pointer] { pointer.reset(); };
    inner();
  };
  outer();
}

void shared_alias_reset_ok() {
  std::shared_ptr<int> pointer(new int(1));
  reset_shared(std::move(pointer));
  if (pointer) {
    int* null_pointer = nullptr;
    *null_pointer = 1;
  }
}

void unique_alias_reset_ok() {
  std::unique_ptr<int> pointer(new int(1));
  reset_unique(std::move(pointer));
  if (pointer) {
    int* null_pointer = nullptr;
    *null_pointer = 1;
  }
}

void nested_alias_reset_ok() {
  std::shared_ptr<int> pointer(new int(1));
  reset_nested(std::move(pointer));
  if (pointer) {
    int* null_pointer = nullptr;
    *null_pointer = 1;
  }
}

int shared_null_after_reset_bad() {
  std::shared_ptr<int> pointer(new int(1));
  reset_shared(std::move(pointer));
  return *pointer;
}

int unique_null_after_reset_bad() {
  std::unique_ptr<int> pointer(new int(1));
  reset_unique(std::move(pointer));
  return *pointer;
}

int shared_release_bad() {
  std::shared_ptr<int> pointer(new int(1));
  int* raw = pointer.get();
  reset_shared(std::move(pointer));
  return *raw;
}

int unique_release_bad() {
  std::unique_ptr<int> pointer(new int(1));
  int* raw = pointer.get();
  reset_unique(std::move(pointer));
  return *raw;
}

void lvalue_reference_reset_ok(std::shared_ptr<int>& pointer) {
  auto&& reset = [&pointer] { pointer.reset(); };
  reset();
}

void value_parameter_reset_ok(std::shared_ptr<int> pointer) {
  auto&& reset = [&pointer] { pointer.reset(); };
  reset();
}

void value_capture_preserves_source_ok() {
  std::shared_ptr<int> pointer(new int(1));
  auto&& reset = [pointer]() mutable { pointer.reset(); };
  reset();
  if (!pointer) {
    int* null_pointer = nullptr;
    *null_pointer = 1;
  }
}

int const_reference_read_ok(const std::shared_ptr<int>&& pointer) {
  auto&& read = [&pointer] { return pointer ? *pointer : 0; };
  return read();
}

struct Deleter {
  void operator()(int* pointer) const { delete pointer; }
};

void reset_custom(std::unique_ptr<int, Deleter>&& pointer) {
  auto&& reset = [&pointer] { pointer.reset(); };
  reset();
}

int custom_deleter_release_bad() {
  std::unique_ptr<int, Deleter> pointer(new int(1));
  int* raw = pointer.get();
  reset_custom(std::move(pointer));
  return *raw;
}

struct NullWritingDeleter {
  void operator()(int* pointer) const {
    *pointer = 1;
    delete pointer;
  }
};

void shared_null_deleter_bad() {
  std::shared_ptr<int> pointer(nullptr, NullWritingDeleter());
  std::shared_ptr<int>&& reference = std::move(pointer);
  auto&& reset = [&reference] { reference.reset(); };
  reset();
}

void unique_null_deleter_ok(
    std::unique_ptr<int, NullWritingDeleter>&& pointer) {
  auto&& reset = [&pointer] { pointer.reset(); };
  reset();
}

void unique_empty_deleter_ok() {
  std::unique_ptr<int, NullWritingDeleter> pointer(nullptr);
  unique_null_deleter_ok(std::move(pointer));
}

void delete_integer(int* pointer) { delete pointer; }

void function_pointer_deleter_ok() {
  std::unique_ptr<int, void (*)(int*)> pointer(new int(1), delete_integer);
  auto&& invoke = [](std::unique_ptr<int, void (*)(int*)>&& pointer) {
    auto&& reset = [&pointer] { pointer.reset(); };
    reset();
  };
  invoke(std::move(pointer));
}

void assign_scalar(int&& value) {
  auto&& assign = [&value] { value = 7; };
  assign();
}

void scalar_alias_ok() {
  int value = 0;
  assign_scalar(std::move(value));
  if (value != 7) {
    int* null_pointer = nullptr;
    *null_pointer = 1;
  }
}

int sibling_null_bad() {
  int* pointer = nullptr;
  return *pointer;
}

} // namespace reference_capture
