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
  File(File&&) noexcept;
  ~File();

  File& operator=(File&&);
  void close();
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

// close() closes the descriptor and leaves -1 behind, so the release() that follows hands
// nothing back and there is no obligation left to drop
int close_then_release_ok(const char* path) {
  const int fd = ::open(path, O_RDONLY);
  if (fd < 0) {
    return -1;
  }
  folly::File file(fd, true);
  file.close();
  (void)file.release();
  return 0;
}

// close() on its own closes it too
int close_only_ok(const char* path) {
  const int fd = ::open(path, O_RDONLY);
  if (fd < 0) {
    return -1;
  }
  folly::File file(fd, true);
  file.close();
  return 0;
}

// TRUE POSITIVE: the descriptor moves to the destination, whose release() hands it back; the
// result is dropped, so it leaks
int moved_then_release_leak_bad(const char* path) {
  const int fd = ::open(path, O_RDONLY);
  if (fd < 0) {
    return -1;
  }
  folly::File file(fd, true);
  folly::File moved(static_cast<folly::File&&>(file));
  (void)moved.release();
  return 0;
}

// the destination closes it, so nothing leaks
int moved_then_close_ok(const char* path) {
  const int fd = ::open(path, O_RDONLY);
  if (fd < 0) {
    return -1;
  }
  folly::File file(fd, true);
  folly::File moved(static_cast<folly::File&&>(file));
  moved.close();
  return 0;
}

// the destination's destructor closes it: silent before and after
int moved_and_kept_ok(const char* path) {
  const int fd = ::open(path, O_RDONLY);
  if (fd < 0) {
    return -1;
  }
  folly::File file(fd, true);
  folly::File moved(static_cast<folly::File&&>(file));
  return use_fd(fd);
}

// TRUE POSITIVE: move ASSIGNMENT carries the descriptor over the same way
int move_assigned_then_release_leak_bad(const char* path) {
  const int fd = ::open(path, O_RDONLY);
  if (fd < 0) {
    return -1;
  }
  folly::File file(fd, true);
  folly::File other;
  other = static_cast<folly::File&&>(file);
  (void)other.release();
  return 0;
}

// TRUE POSITIVE: moving a File onto itself does nothing, so it still owns the descriptor and
// the release() hands it back
int self_move_assigned_then_release_leak_bad(const char* path) {
  const int fd = ::open(path, O_RDONLY);
  if (fd < 0) {
    return -1;
  }
  folly::File file(fd, true);
  file = static_cast<folly::File&&>(file);
  (void)file.release();
  return 0;
}

} // namespace folly_file_fd_ownership
