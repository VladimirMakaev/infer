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

  // These two are INLINE in folly and read fd_, which only the out-of-line
  // members above ever write. A model standing in for one of those is the only
  // thing that can initialize fd_, so these are what notice when it does not.
  int fd() const { return fd_; }
  explicit operator bool() const { return fd_ != -1; }

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

// fd_ is written by the out-of-line constructor, which the model stands in for, so the inline
// fd() and operator bool() must not read an uninitialized value.
//
// WHAT PINS THAT is the group below this one, where the File crosses a procedure boundary. The
// same-procedure cases here were silent even before the constructor model wrote fd_, so on
// their own they would let a revert of that write through; they are kept because they are the
// shapes the report was filed about, not because they test anything.
int reads_the_fd_through_the_file_ok(const char* path) {
  const int fd = ::open(path, O_RDONLY);
  if (fd < 0) {
    return -1;
  }
  const folly::File file(fd, true);
  if (file) {
    return use_fd(file.fd());
  }
  return -2;
}

// the same without ownership: the fields are still written, and the caller closes the
// descriptor it kept
int reads_the_fd_without_owning_ok(const char* path) {
  const int fd = ::open(path, O_RDONLY);
  if (fd < 0) {
    return -1;
  }
  const folly::File file(fd);
  const int seen = file ? use_fd(file.fd()) : -2;
  ::close(fd);
  return seen;
}

// a default-constructed File has an inline constructor, so this always worked
int reads_the_fd_of_a_default_file_ok() {
  const folly::File file;
  return file ? file.fd() : -1;
}

// close() and release() leave -1 behind, so reading the File afterwards is defined
int reads_the_fd_after_close_ok(const char* path) {
  const int fd = ::open(path, O_RDONLY);
  if (fd < 0) {
    return -1;
  }
  folly::File file(fd, true);
  file.close();
  return file ? file.fd() : -1;
}

int reads_the_fd_after_release_ok(const char* path) {
  const int fd = ::open(path, O_RDONLY);
  if (fd < 0) {
    return -1;
  }
  folly::File file(fd, true);
  const int released = file.release();
  const int seen = file ? file.fd() : -1;
  ::close(released);
  return seen;
}

// a move carries the fields over, so both sides are readable afterwards
int reads_the_fd_after_move_ok(const char* path) {
  const int fd = ::open(path, O_RDONLY);
  if (fd < 0) {
    return -1;
  }
  folly::File file(fd, true);
  folly::File moved(static_cast<folly::File&&>(file));
  const int seen = moved ? use_fd(moved.fd()) : -2;
  return file ? seen : seen;
}

// THE CASES THAT PIN THE fd_ WRITE.
//
// A File constructed in ANOTHER procedure is what the production report was really about: the
// callee's summary carries whatever the constructor model left, and the caller's read of fd_ is
// then checked against it. Construct and read in one procedure and nothing is reported either
// way, which is why the group above pins nothing. Every case below reported
// "`this.fd_` is read without initialization" before the constructor model wrote fd_, and is
// silent now; revert that write and each one comes back.

// the shape the production code has: a helper that opens a descriptor and hands back an owning
// File, whose caller asks whether it is open and reads it
folly::File openDirectory(const char* path) {
  return folly::File(::open(path, O_RDONLY | O_DIRECTORY), /*ownsFd=*/true);
}

int reads_a_returned_file_ok(const char* path) {
  const folly::File tree = openDirectory(path);
  if (!tree) {
    return -1;
  }
  return use_fd(tree.fd());
}

// both reads in one condition, as the production code writes it
int reads_a_returned_file_in_a_condition_ok(const char* path) {
  const folly::File tree = openDirectory(path);
  if (!tree || use_fd(tree.fd()) != 0) {
    return -1;
  }
  return 0;
}

// the read happens one procedure further down, through a reference
int inspect_the_file(const folly::File& file) {
  if (!file) {
    return -1;
  }
  return use_fd(file.fd());
}

int reads_a_returned_file_by_reference_ok(const char* path) {
  const folly::File project = openDirectory(path);
  return inspect_the_file(project);
}

// the same across a boundary for a File that does NOT own its descriptor, which takes the
// other branch of the model: the caller keeps the descriptor and closes it
folly::File borrowDescriptor(int fd) { return folly::File(fd); }

int reads_a_returned_borrowed_file_ok(const char* path) {
  const int fd = ::open(path, O_RDONLY);
  if (fd < 0) {
    return -1;
  }
  const folly::File file = borrowDescriptor(fd);
  const int seen = file ? use_fd(file.fd()) : -2;
  ::close(fd);
  return seen;
}

// CONTROL: a field a translated constructor really does leave uninitialized is still reported
// through the same kind of inline accessor, on both sides of this change, so this is not
// blanket initialization of whatever a model touches.
struct NotAFile {
  int fd_;
  int flags_ = 0;
  NotAFile() {}
  int fd() const { return fd_; }
};

int reads_an_uninitialized_field_bad() {
  NotAFile n{};
  return n.fd();
}

} // namespace folly_file_fd_ownership
