/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <fcntl.h>
#include <unistd.h>
#include <cstdint>
#include <cstdlib>
#include <memory>

namespace fd_constructor {
void use_fd(int);

struct Owner {
  explicit Owner(int);
  ~Owner();
  void observe(int);
};
struct Observer {
  explicit Observer(int);
};
struct Member {
  ~Member();
};
struct ImplicitDestructor {
  Member member;
  explicit ImplicitDestructor(int);
};
struct InlineOwner {
  int fd;
  explicit InlineOwner(int fd) : fd(fd) {}
  ~InlineOwner() { close(fd); }
};
struct InlineObserver {
  explicit InlineObserver(int) {}
  ~InlineObserver() {}
};
struct Nonclosing {
  explicit Nonclosing(int);
  ~Nonclosing() {}
};
struct IntegerOwner {
  explicit IntegerOwner(std::intptr_t);
  ~IntegerOwner();
};

void B1_leak_after_unknown_use_bad(const char* path) {
  int fd = open(path, O_RDONLY);
  if (fd >= 0)
    use_fd(fd);
}
void B2_leak_into_non_owner_ctor_bad(const char* path) {
  int fd = open(path, O_RDONLY);
  if (fd >= 0) {
    Observer observer(fd);
  }
}
void B3_handed_to_owner_ctor_ok(const char* path) {
  int fd = open(path, O_RDONLY);
  if (fd >= 0) {
    Owner owner(fd);
  }
}
void shared_owner_ok(const char* path) {
  int fd = open(path, O_RDONLY);
  if (fd >= 0) {
    auto owner = std::make_shared<Owner>(fd);
  }
}
void implicit_destructor_bad(const char* path) {
  int fd = open(path, O_RDONLY);
  if (fd >= 0) {
    ImplicitDestructor observer(fd);
  }
}
void unknown_method_bad(const char* path, Owner& owner) {
  int fd = open(path, O_RDONLY);
  if (fd >= 0)
    owner.observe(fd);
}
void inline_owner_ok(const char* path) {
  int fd = open(path, O_RDONLY);
  if (fd >= 0) {
    InlineOwner owner(fd);
  }
}
void inline_nonclosing_bad(const char* path) {
  int fd = open(path, O_RDONLY);
  if (fd >= 0) {
    InlineObserver observer(fd);
  }
}
// An unknown constructor with a user-declared destructor is assumed to adopt
// the fd even when its destructor never closes it. This leak is not detected.
void FN_nonclosing_unknown_constructor_bad(const char* path) {
  int fd = open(path, O_RDONLY);
  if (fd >= 0) {
    Nonclosing observer(fd);
  }
}
void integer_cast_memory_bad() {
  auto p = malloc(16);
  IntegerOwner owner(reinterpret_cast<std::intptr_t>(p));
}
void adopt_fd(int fd) { Owner owner(fd); }
void summarized_owner_ok(const char* path) {
  int fd = open(path, O_RDONLY);
  if (fd >= 0)
    adopt_fd(fd);
}
void pass_integer(std::intptr_t value) { IntegerOwner owner(value); }
void summarized_integer_cast_memory_bad() {
  auto p = malloc(16);
  pass_integer(reinterpret_cast<std::intptr_t>(p));
}
void maybe_adopt_fd(int fd, bool adopt) {
  if (adopt) {
    Owner owner(fd);
  }
}
void conditional_adoption_bad(const char* path, bool adopt) {
  int fd = open(path, O_RDONLY);
  if (fd >= 0)
    maybe_adopt_fd(fd, adopt);
}
struct ReferenceObserver {
  explicit ReferenceObserver(int&);
  ~ReferenceObserver();
};
void reference_constructor_bad(const char* path) {
  int fd = open(path, O_RDONLY);
  if (fd >= 0) {
    ReferenceObserver observer(fd);
  }
}
struct DoubleClose {
  InlineOwner owner;
  explicit DoubleClose(int fd) : owner(fd) {}
  ~DoubleClose() { close(owner.fd); }
};
void TP_raii_then_explicit_close_bad(const char* path) {
  int fd = open(path, O_RDONLY);
  if (fd >= 0) {
    DoubleClose owner(fd);
  }
}
} // namespace fd_constructor
