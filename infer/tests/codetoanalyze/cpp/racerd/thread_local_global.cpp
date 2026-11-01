/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// A [thread_local] variable is private to its thread, so accesses to it cannot
// race. Nothing in the AST Infer receives says a variable is thread-local --
// the clang AST exporter does not export the storage duration -- so these read
// as ordinary globals and are reported.

#include <mutex>

namespace thread_local_global {

struct Message {
  Message() : length_(0) {}
  int length_;
};

struct Plain {
  int length_;
};

thread_local Message gMessage;
thread_local Plain gPlain;
thread_local int gCount;
extern thread_local Message gExternMessage;
Message gShared;
thread_local Message* gPointerToShared;

// The constructor makes no difference: the aggregate below behaves the same.
class ConstructedHolder {
 public:
  void write() {
    std::lock_guard<std::mutex> guard(mutex_);
    gMessage.length_ = 1;
  }
  int read_ok() { return gMessage.length_; }

 private:
  std::mutex mutex_;
};

// The storage duration is what matters, not whether the type is trivial.
class AggregateHolder {
 public:
  void write() {
    std::lock_guard<std::mutex> guard(mutex_);
    gPlain.length_ = 1;
  }
  int read_ok() { return gPlain.length_; }

 private:
  std::mutex mutex_;
};

// A declaration carries the storage duration just as a definition does.
class ExternHolder {
 public:
  void write() {
    std::lock_guard<std::mutex> guard(mutex_);
    gExternMessage.length_ = 1;
  }
  int read_ok() { return gExternMessage.length_; }

 private:
  std::mutex mutex_;
};

// A static member declared thread_local is the same case.
class MemberHolder {
 public:
  static thread_local Message member_;
  void write() {
    std::lock_guard<std::mutex> guard(mutex_);
    member_.length_ = 1;
  }
  int read_ok() { return member_.length_; }

 private:
  std::mutex mutex_;
};
thread_local Message MemberHolder::member_;

// A thread-local scalar is already silent, but not because it is thread-local:
// a global scalar reached without a field access is not reported either way.
// shared_scalar_ok below is the control for that.
class ScalarHolder {
 public:
  void write() {
    std::lock_guard<std::mutex> guard(mutex_);
    gCount = 1;
  }
  int read_ok() { return gCount; }

 private:
  std::mutex mutex_;
};

int gSharedCount;

class SharedScalarHolder {
 public:
  void write() {
    std::lock_guard<std::mutex> guard(mutex_);
    gSharedCount = 1;
  }
  int shared_scalar_ok() { return gSharedCount; }

 private:
  std::mutex mutex_;
};

// A function-local thread_local, which the base does not report either.
int use_function_local() {
  static thread_local Message message;
  message.length_ = 1;
  return message.length_;
}

class FunctionLocalHolder {
 public:
  void write() {
    std::lock_guard<std::mutex> guard(mutex_);
    use_function_local();
  }
  int read_ok() { return use_function_local(); }

 private:
  std::mutex mutex_;
};

// The pointer is thread-local but what it points at is shared, so the pointee
// races.
class PointeeHolder {
 public:
  void write() {
    std::lock_guard<std::mutex> guard(mutex_);
    gPointerToShared->length_ = 1;
  }
  int read_bad() { return gPointerToShared->length_; }

 private:
  std::mutex mutex_;
};

// The control: an ordinary shared global of the same type must keep reporting.
class SharedHolder {
 public:
  void write() {
    std::lock_guard<std::mutex> guard(mutex_);
    gShared.length_ = 1;
  }
  int read_bad() { return gShared.length_; }

 private:
  std::mutex mutex_;
};
} // namespace thread_local_global
