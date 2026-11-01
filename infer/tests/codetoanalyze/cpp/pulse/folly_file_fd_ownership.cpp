/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// folly::File takes ownership of a descriptor when its second argument is true and closes it in
// its destructor. The constructor is declared out of line, so without a model the descriptor
// looks leaked at every early return. Ownership is forgotten rather than released: the
// descriptor stays usable while the File lives. release() hands the descriptor back, so it
// hands the obligation to close it back too.

#include <fcntl.h>
#include <unistd.h>

namespace folly {
class File {
 public:
  constexpr File() noexcept : fd_(-1), ownsFd_(false) {}
  explicit File(int fd, bool ownsFd = false) noexcept;
  ~File();

  int release() noexcept;

 private:
  int fd_;
  bool ownsFd_;
};
} // namespace folly

extern int use_fd(int fd);

namespace folly_file_fd_ownership {

// the File owns the descriptor, so the early return leaks nothing
int owns_the_fd_ok(const char* path) {
  const int fd = ::open(path, O_RDONLY);
  if (fd < 0) {
    return -1;
  }
  const folly::File file(fd, true);
  if (use_fd(fd) != 0) {
    return -2;
  }
  return 0;
}

// the descriptor is still usable after the File is constructed
int uses_the_fd_after_ok(const char* path) {
  const int fd = ::open(path, O_RDONLY);
  if (fd < 0) {
    return -1;
  }
  const folly::File file(fd, true);
  return use_fd(fd);
}

// ownsFd is false, so the descriptor stays the caller's and nobody closes it
int does_not_own_the_fd_bad(const char* path) {
  const int fd = ::open(path, O_RDONLY);
  if (fd < 0) {
    return -1;
  }
  const folly::File file(fd, false);
  return 0;
}

// the default is ownsFd = false
int default_does_not_own_bad(const char* path) {
  const int fd = ::open(path, O_RDONLY);
  if (fd < 0) {
    return -1;
  }
  const folly::File file(fd);
  return 0;
}

// release() gives the descriptor back to the caller, and nobody closes it afterwards
int release_then_leak_bad(const char* path) {
  const int fd = ::open(path, O_RDONLY);
  if (fd < 0) {
    return -1;
  }
  folly::File file(fd, true);
  file.release();
  return 0;
}

// the descriptor release() handed back is closed
int release_then_close_ok(const char* path) {
  const int fd = ::open(path, O_RDONLY);
  if (fd < 0) {
    return -1;
  }
  folly::File file(fd, true);
  const int released = file.release();
  ::close(released);
  return 0;
}

// the usual shape: the descriptor release() handed back is the caller's problem now
int release_then_return_ok(const char* path) {
  const int fd = ::open(path, O_RDONLY);
  if (fd < 0) {
    return -1;
  }
  folly::File file(fd, true);
  return file.release();
}

// no File at all: the plain leak still reports
int plain_open_bad(const char* path) {
  const int fd = ::open(path, O_RDONLY);
  if (fd < 0) {
    return -1;
  }
  return 0;
}

} // namespace folly_file_fd_ownership
