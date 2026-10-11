/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <array>
#include <atomic>
#include <cstdint>

struct ByteIndexGrid {
  struct Cell {
    std::atomic<unsigned long> count{0};
  };

  std::array<std::array<Cell, 64>, 5> cells{};

  void outer_index_bad(std::uint8_t outer, unsigned inner) {
    if (inner < 64) {
      auto& row = cells[outer];
      row[inner].count += 1;
    }
  }

  void both_indices_guarded_ok(std::uint8_t outer, unsigned inner) {
    if (outer < 5 && inner < 64) {
      cells[outer][inner].count += 1;
    }
  }

  void early_outer_guard_ok(std::uint8_t outer, unsigned inner) {
    if (outer >= 5) {
      return;
    }
    if (inner < 64) {
      cells[outer][inner].count += 1;
    }
  }

  void masked_outer_ok(std::uint8_t outer, unsigned inner) {
    if (inner < 64) {
      cells[outer & 3][inner].count += 1;
    }
  }

  void inner_guard_ok(unsigned inner) {
    if (inner < 64) {
      cells[4][inner].count += 1;
    }
  }

  void valid_index_ok() { cells[4][63].count += 1; }
  void constant_outer_bad() { cells[5][0].count += 1; }
  void constant_inner_bad() { cells[0][64].count += 1; }

  void reset_ok() {
    for (unsigned i = 0; i < cells.size(); ++i) {
      auto& row = cells[i];
      for (unsigned j = 0; j < row.size(); ++j) {
        row[j].count = 0;
      }
    }
  }

  void reset_direct_ok() {
    for (unsigned i = 0; i < cells.size(); ++i) {
      for (unsigned j = 0; j < cells[i].size(); ++j) {
        cells[i][j].count = 0;
      }
    }
  }

  // The byte-index reporting rule does not apply to throwing accessors.
  void throwing_at_ok(std::uint8_t outer) { (void)cells.at(outer); }

  void bool_outer_ok(bool outer, unsigned inner) {
    if (inner < 64) {
      cells[outer][inner].count += 1;
    }
  }

  // Other symbolic-index reporting policies are deliberately unchanged.
  void FN_wide_outer_bad(unsigned outer, unsigned inner) {
    if (inner < 64) {
      cells[outer][inner].count += 1;
    }
  }

  void FN_signed_outer_bad(signed char outer, unsigned inner) {
    if (inner < 64) {
      cells[outer][inner].count += 1;
    }
  }

  void FN_short_outer_bad(unsigned short outer, unsigned inner) {
    if (inner < 64) {
      cells[outer][inner].count += 1;
    }
  }
};

void byte_grid_caller_outer_bad() {
  ByteIndexGrid grid;
  grid.outer_index_bad(5, 0);
}

void byte_grid_caller_valid_ok() {
  ByteIndexGrid grid;
  grid.outer_index_bad(4, 63);
}

struct FullByteIndexGrid {
  std::array<std::array<int, 64>, 256> cells{};

  // Every uint8 value is a valid outer index.
  int full_byte_range_ok(std::uint8_t outer, unsigned inner) {
    if (inner < 64) {
      return cells[outer][inner];
    }
    return 0;
  }
};

// Scalar and native arrays keep their existing symbolic-index policy.
void FN_byte_grid_scalar_outer_bad(std::uint8_t outer) {
  std::array<int, 5> cells{};
  cells[outer] = 1;
}

void FN_byte_grid_native_outer_bad(std::uint8_t outer) {
  int cells[5]{};
  cells[outer] = 1;
}

// A pointer to a row has an unknown extent/offset, not the outer array's block.
void FN_byte_grid_reference_outer_bad(
    std::array<std::array<int, 64>, 5>& cells, std::uint8_t outer) {
  (void)cells[outer];
}
