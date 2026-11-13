/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <dirent.h>
#include <unistd.h>

namespace leak_reported_once {

int consume(int);

// One handle left behind on several ways out whose last access differs. That is one leak and
// one report, named at the earliest of those ways out.
//
// A procedure can be analysed more than once, once per specialization, and each of those
// analyses begins with no memory of what the others reported, so entries describing this one
// leak at a different way out each time can all reach the error log, which keys an entry on its
// message and cannot tell they are one finding. They are collapsed when the report is written,
// by the allocation they are about.
int one_dir_several_exits_bad(const char* path, int c) {
  DIR* dir = opendir(path);
  if (dir == nullptr) {
    return -1;
  }
  readdir(dir);
  if (c > 10) {
    return 0;
  }
  readdir(dir);
  if (c > 3) {
    return 1;
  }
  readdir(dir);
  return 2;
}

// Two handles opened at two places, each left behind. These are two allocations and must stay
// two reports. They agree on everything the report shows except which line opened the handle --
// same procedure, same issue type, same resource, and even the same line for the last access --
// so anything coarser than the allocation itself would merge them and lose a leak.
int two_dirs_bad(const char* first, const char* second, int c) {
  DIR* a = opendir(first);
  DIR* b = opendir(second);
  if (a == nullptr || b == nullptr) {
    return -1;
  }
  readdir(a);
  readdir(b);
  return consume(c);
}

// Two descriptors made by ONE call. Pulse gives the pair a single allocation, so this is one
// report rather than two, and there is nothing here for the collapse above to merge.
int pipe_one_call_bad(int c) {
  int fds[2];
  if (pipe(fds) != 0) {
    return -1;
  }
  return consume(fds[0]) + consume(fds[1]);
}

int one_dir_closed_ok(const char* path, int c) {
  DIR* dir = opendir(path);
  if (dir == nullptr) {
    return -1;
  }
  readdir(dir);
  if (c > 3) {
    closedir(dir);
    return 1;
  }
  readdir(dir);
  closedir(dir);
  return 2;
}

} // namespace leak_reported_once
