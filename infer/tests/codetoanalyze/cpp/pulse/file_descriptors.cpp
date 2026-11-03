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

// The vector does not own file descriptors, so destroying it leaves
// this descriptor open.
void push_back_fd_into_local_vector_bad(const char* path) {
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

// Hands back a descriptor at or above a minimum and closes the one it was given.
static int move_descriptor_high(int fd) {
  if (fd < 0) {
    return -1;
  }
  const int moved = fcntl(fd, F_DUPFD_CLOEXEC, 10);
  close(fd);
  return moved;
}

// Two calls return two different descriptors, so closing both is not a double
// close, even on the path where the descriptor they are given is the same.
int two_moved_descriptors_ok() {
  std::array<int, 2> fds{-1, -1};
  if (pipe(fds.data()) != 0) {
    return -1;
  }
  const int reader = move_descriptor_high(fds[0]);
  const int writer = move_descriptor_high(fds[1]);
  if (reader >= 0) {
    close(reader);
  }
  if (writer >= 0) {
    close(writer);
  }
  return 0;
}

// The same through a holder that closes and then overwrites its descriptor.
class MovedFd {
 public:
  explicit MovedFd(int fd = -1) : fd_(fd) {}
  ~MovedFd() { reset(); }
  MovedFd(MovedFd&& other) noexcept : fd_(other.release()) {}
  MovedFd& operator=(MovedFd&& other) noexcept {
    reset(other.release());
    return *this;
  }
  int release() {
    const int fd = fd_;
    fd_ = -1;
    return fd;
  }
  void reset(int fd = -1);

 private:
  int fd_;
};

void MovedFd::reset(int fd) {
  if (fd_ >= 0) {
    close(fd_);
  }
  fd_ = fd;
}

static MovedFd move_descriptor_high_owned(int fd) {
  if (fd < 0) {
    return MovedFd();
  }
  const int moved = fcntl(fd, F_DUPFD_CLOEXEC, 10);
  close(fd);
  return MovedFd(moved);
}

int two_moved_descriptors_owned_ok() {
  std::array<int, 2> fds{-1, -1};
  MovedFd reader;
  MovedFd writer;
  if (pipe(fds.data()) == 0) {
    reader = move_descriptor_high_owned(fds[0]);
    writer = move_descriptor_high_owned(fds[1]);
  }
  return 0;
}

// The control: one descriptor closed twice is still a double close.
void close_twice_bad(const char* path) {
  const int fd = open(path, O_RDONLY);
  if (fd >= 0) {
    close(fd);
    close(fd);
  }
}

// A descriptor a holder closed and did not clear is still closed twice.
void close_then_holder_closes_again_bad(const char* path) {
  MovedFd owner(open(path, O_RDONLY));
  const int fd = owner.release();
  if (fd >= 0) {
    close(fd);
    close(fd);
  }
}

// Asio wraps [close] in a retry: when the call fails with EWOULDBLOCK or EAGAIN it puts the
// descriptor back into blocking mode with [ioctl] and closes it again. [close] leaves its return
// value unconstrained on the branch that releases the descriptor, so that retry is a feasible
// path on which the descriptor is used and released a second time, and the report lands on
// whoever owns the socket. Giving the wrapper a model releases the descriptor exactly once and
// leaves the retry unanalysed.
//
// Declared and not defined on purpose. A definition here would be analysed directly and the
// report would land inside the wrapper, where no call-site model can prevent it; the wrapper
// lives in a third-party header in a real build. Declared, the wrapper is an unknown call on the
// unfixed build, so each of the _ok functions below leaks its descriptor there and is silent
// here -- which is what pins the model.
namespace boost {
namespace asio {
namespace detail {
namespace socket_ops {
int close(int s, unsigned char& state, bool destruction, int& ec);
}
namespace descriptor_ops {
int close(int d, unsigned char& state, int& ec);
}
} // namespace detail
} // namespace asio
} // namespace boost

// the standalone distribution of Asio, whose namespace has no [boost] prefix
namespace asio {
namespace detail {
namespace descriptor_ops {
int close(int d, unsigned char& state, int& ec);
}
} // namespace detail
} // namespace asio

void asio_socket_closed_once_ok(const char* path) {
  const int s = open(path, O_RDONLY);
  unsigned char state = 0;
  int ec = 0;
  boost::asio::detail::socket_ops::close(s, state, true, ec);
}

void asio_descriptor_closed_once_ok(const char* path) {
  const int d = open(path, O_RDONLY);
  unsigned char state = 0;
  int ec = 0;
  boost::asio::detail::descriptor_ops::close(d, state, ec);
}

void standalone_asio_descriptor_closed_once_ok(const char* path) {
  const int d = open(path, O_RDONLY);
  unsigned char state = 0;
  int ec = 0;
  asio::detail::descriptor_ops::close(d, state, ec);
}

// The control for the other direction: the wrapper still releases the descriptor, so closing it
// again afterwards is still a double close.
void asio_close_then_close_again_bad(const char* path) {
  const int s = open(path, O_RDONLY);
  if (s >= 0) {
    unsigned char state = 0;
    int ec = 0;
    boost::asio::detail::socket_ops::close(s, state, true, ec);
    close(s);
  }
}
