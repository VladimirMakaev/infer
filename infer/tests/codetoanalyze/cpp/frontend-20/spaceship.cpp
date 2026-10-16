/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// A three-way comparison needs a comparison category to exist, but the expected
// CFG must not contain a standard-library body, so the category is declared
// here instead of included, and its comparisons are left undefined.
namespace std {
struct strong_ordering {
  signed char v;
  static const strong_ordering less;
  static const strong_ordering equal;
  static const strong_ordering equivalent;
  static const strong_ordering greater;
};
inline constexpr strong_ordering strong_ordering::less{-1};
inline constexpr strong_ordering strong_ordering::equal{0};
inline constexpr strong_ordering strong_ordering::equivalent{0};
inline constexpr strong_ordering strong_ordering::greater{1};
bool operator==(strong_ordering, int) noexcept;
bool operator<(strong_ordering, int) noexcept;
} // namespace std

// The builtin three-way comparison: its result is not modelled, but the
// enclosing function is translated.
void builtin_three_way(int a, int b) {
  auto c = a <=> b;
  (void)c;
}

struct Point {
  int x;
  // The defaulted operator compares the members with the builtin three-way
  // operator, so its synthesised body is the one that used to be dropped.
  auto operator<=>(const Point&) const = default;
};

void defaulted_three_way(const Point& p, const Point& q) {
  auto c = p <=> q;
  (void)c;
}

bool compare_points(const Point& p, const Point& q) { return (p <=> q) < 0; }
