/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <array>
#include <fcntl.h>
#include <functional>
#include <memory>
#include <unistd.h>
#include <vector>

int pipe_array_data_ok() {
  std::array<int, 2> fds;
  if (pipe(fds.data()) == -1) {
    return -1;
  }
  close(fds[0]);
  close(fds[1]);
  return 0;
}

int pipe_vector_data_ok() {
  std::vector<int> fds(2);
  if (pipe(fds.data()) == -1) {
    return -1;
  }
  close(fds[0]);
  close(fds[1]);
  return 0;
}

// `std::array::data()` is unknown to Pulse, which does not track descriptors
// stored through the pointer it returns
int FN_pipe_array_data_not_closed_bad() {
  std::array<int, 2> fds;
  return pipe(fds.data());
}

void std_function_takes_fd_ok(const std::function<void(int)>& f,
                              const char* path) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    return;
  }
  f(fd);
}

void push_back_fd_ok(std::vector<int>& fds, const char* path) {
  int fd = open(path, O_RDONLY);
  if (fd == -1) {
    return;
  }
  fds.push_back(fd);
}

void emplace_back_fd_ok(std::vector<int>& fds, const char* path) {
  int fd = open(path, O_RDONLY);
  if (fd == -1) {
    return;
  }
  fds.emplace_back(fd);
}

// the descriptor stored in the local vector is not reported when the vector
// goes away
void FN_push_back_fd_into_local_vector_bad(const char* path) {
  std::vector<int> fds;
  int fd = open(path, O_RDONLY);
  if (fd == -1) {
    return;
  }
  fds.push_back(fd);
}

void register_object(void* obj);

struct RegisteredFdOwner {
  int fd;
  explicit RegisteredFdOwner(int f) : fd(f) { register_object(this); }
  ~RegisteredFdOwner() { close(fd); }
};

std::unique_ptr<RegisteredFdOwner> constructor_stores_fd_and_registers_this_ok(
    const char* path) {
  int fd = open(path, O_RDWR);
  if (fd < 0) {
    return nullptr;
  }
  return std::unique_ptr<RegisteredFdOwner>(new RegisteredFdOwner(fd));
}

RegisteredFdOwner* new_owner_stores_fd_and_registers_this_ok(const char* path) {
  int fd = open(path, O_RDWR);
  if (fd < 0) {
    return nullptr;
  }
  return new RegisteredFdOwner(fd);
}
