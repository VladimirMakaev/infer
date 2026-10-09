/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <unistd.h>
#include <cstdint>
#include <cstdlib>
#define FD_OWNER __attribute__((annotate("infer_takes_fd_ownership")))
namespace fd_contracts {
void take_fd(int fd FD_OWNER);
void inspect_fd(int fd);
void take_second(int observed, int owned FD_OWNER);
void take_fourth(int observed, const char*, int, int owned FD_OWNER);
int take_with_status(int owned FD_OWNER);
void different_annotation(int fd __attribute__((annotate("other_metadata"))));
void inspect_reference(int& fd FD_OWNER) {
  if (fd >= 0) {
  }
}
void take_integer(std::intptr_t fd FD_OWNER);
void known_close(int fd FD_OWNER) { close(fd); }
void known_observe(int fd) {
  if (fd >= 0) {
  }
}
void known_bad_contract(int fd FD_OWNER) {
  if (fd >= 0) {
  }
}
struct Owner {
  void accept(int fd FD_OWNER);
  virtual void accept_two(int one FD_OWNER, int two FD_OWNER) = 0;
  void mixed(int observed, int owned FD_OWNER);
};
struct Observer {
  void accept(int fd);
};
struct FdHolder {
  explicit FdHolder(int fd FD_OWNER);
  ~FdHolder();
};
void non_fd_resource_bad() {
  void* p = malloc(16);
  take_integer(reinterpret_cast<std::intptr_t>(p));
}
void direct_owner_ok(int src) { take_fd(dup(src)); }
void method_owner_ok(int src, Owner& sink) { sink.accept(dup(src)); }
void virtual_owner_ok(int src, Owner& sink) {
  sink.accept_two(dup(src), dup(src));
}
void constructor_owner_ok(int src) { FdHolder holder{dup(src)}; }
void fourth_owner_ok(int src) { take_fourth(0, "x", 1, dup(src)); }
void mixed_arguments_bad(int src) { take_second(dup(src), dup(src)); }
void fourth_mixed_bad(int src) { take_fourth(dup(src), "x", 1, dup(src)); }
void method_mixed_bad(int src, Owner& sink) { sink.mixed(dup(src), dup(src)); }
void direct_observer_bad(int src) { inspect_fd(dup(src)); }
void same_name_observer_bad(int src, Observer& sink) { sink.accept(dup(src)); }
void known_observer_bad(int src) { known_observe(dup(src)); }
void unused_dup_bad(int src) {
  int fd = dup(src);
  (void)fd;
}
void different_annotation_bad(int src) { different_annotation(dup(src)); }
void by_reference_bad(int src) {
  int fd = dup(src);
  inspect_reference(fd);
}
void known_bad_contract_bad(int src) { known_bad_contract(dup(src)); }
void known_close_ok(int src) { known_close(dup(src)); }
void status_zero_path_bad(int src) {
  int fd = dup(src);
  if (fd <= 0) {
    if (fd == 0)
      close(fd);
    return;
  }
  int status = take_with_status(fd);
  if (status == 0) {
    int* p = nullptr;
    *p = 1;
  }
}
void status_nonzero_path_bad(int src) {
  int fd = dup(src);
  if (fd <= 0) {
    if (fd == 0)
      close(fd);
    return;
  }
  int status = take_with_status(fd);
  if (status != 0) {
    int* p = nullptr;
    *p = 1;
  }
}
} // namespace fd_contracts
