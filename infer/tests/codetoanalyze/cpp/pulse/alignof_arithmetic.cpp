/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstddef>
#include <cstring>

// `alignof(T)` is a compile-time constant, and the frontend translates it as one. If it were an
// unknown instead, `round_up` below would divide by an unknown, which cannot be folded: the
// rounded value of an already-aligned pointer would not be known equal to that pointer, so the
// `padding != 0` guard would stay satisfiable and the fill would be explored on a null range.

namespace alignof_arithmetic {

std::size_t round_up(std::size_t n, std::size_t align) {
  return ((n + align - 1) / align) * align;
}

struct Padded {
  char c;
  int i;
}; // alignof 4, sizeof 8: a type whose alignment is NOT its size

struct alignas(32) OverAligned {
  char c;
}; // alignment larger than any member needs

// An empty range: begin is null, so round_up leaves it alone and the fill is unreachable.
void fill_float(void* begin) {
  std::size_t aligned =
      round_up(reinterpret_cast<std::size_t>(begin), alignof(float));
  std::ptrdiff_t padding =
      reinterpret_cast<char*>(aligned) - static_cast<char*>(begin);
  if (padding != 0) {
    memset(begin, 0, padding);
  }
}

void empty_float_range_ok() { fill_float(nullptr); }

// The same, for a type whose alignment differs from its size. Folding `alignof` to the SIZE
// would put 8 here instead of 4 and still be silent, so this case alone does not pin the
// difference -- `padded_offset_by_align_bad` below is what does.
void fill_padded(void* begin) {
  std::size_t aligned =
      round_up(reinterpret_cast<std::size_t>(begin), alignof(Padded));
  std::ptrdiff_t padding =
      reinterpret_cast<char*>(aligned) - static_cast<char*>(begin);
  if (padding != 0) {
    memset(begin, 0, padding);
  }
}

void empty_padded_range_ok() { fill_padded(nullptr); }

void fill_over_aligned(void* begin) {
  std::size_t aligned =
      round_up(reinterpret_cast<std::size_t>(begin), alignof(OverAligned));
  std::ptrdiff_t padding =
      reinterpret_cast<char*>(aligned) - static_cast<char*>(begin);
  if (padding != 0) {
    memset(begin, 0, padding);
  }
}

void empty_over_aligned_range_ok() { fill_over_aligned(nullptr); }

void fill_pointer(void* begin) {
  std::size_t aligned =
      round_up(reinterpret_cast<std::size_t>(begin), alignof(void*));
  std::ptrdiff_t padding =
      reinterpret_cast<char*>(aligned) - static_cast<char*>(begin);
  if (padding != 0) {
    memset(begin, 0, padding);
  }
}

void empty_pointer_range_ok() { fill_pointer(nullptr); }

// TRUE POSITIVE. The offset is the alignment itself, so the guard is true and the null range is
// really written. Must keep reporting: the change makes `alignof` known, and a known value
// decides this branch the other way.
void fill_offset_by_align(void* begin) {
  std::size_t moved = reinterpret_cast<std::size_t>(begin) + alignof(float);
  std::ptrdiff_t padding =
      reinterpret_cast<char*>(moved) - static_cast<char*>(begin);
  if (padding != 0) {
    memset(begin, 0, padding);
  }
}

void offset_by_align_bad() { fill_offset_by_align(nullptr); }

// TRUE POSITIVE, and the one case that tells a folded ALIGNMENT from a folded SIZE: for Padded
// the alignment is 4 and the size is 8, so this writes only when the value really is 4.
void fill_when_align_is_four(void* begin) {
  if (alignof(Padded) == 4) {
    memset(begin, 0, 1);
  }
}

void padded_offset_by_align_bad() { fill_when_align_is_four(nullptr); }

// The mirror of the above: unreachable, because the alignment of Padded is not its size. An
// implementation that folded `alignof` to `sizeof` would report here.
void fill_when_align_is_eight(void* begin) {
  if (alignof(Padded) == 8) {
    memset(begin, 0, 1);
  }
}

void padded_not_size_ok() { fill_when_align_is_eight(nullptr); }

// `__alignof__` is the GNU spelling, which clang reports as a preferred alignment.
void fill_when_preferred_align_is_four(void* begin) {
  if (__alignof__(Padded) == 4) {
    memset(begin, 0, 1);
  }
}

void preferred_align_bad() { fill_when_preferred_align_is_four(nullptr); }

// The GNU spelling on the silent side. The case above reports whether or not the preferred
// alignment is known -- an unknown can be four -- so it is this one that needs the value: an
// unknown divisor leaves the rounded pointer unconstrained and the fill is explored again.
void fill_preferred(void* begin) {
  std::size_t aligned =
      round_up(reinterpret_cast<std::size_t>(begin), __alignof__(Padded));
  std::ptrdiff_t padding =
      reinterpret_cast<char*>(aligned) - static_cast<char*>(begin);
  if (padding != 0) {
    memset(begin, 0, padding);
  }
}

void empty_preferred_range_ok() { fill_preferred(nullptr); }

} // namespace alignof_arithmetic
