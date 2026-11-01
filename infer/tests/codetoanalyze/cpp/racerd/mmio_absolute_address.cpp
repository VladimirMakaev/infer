/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Firmware reaches a memory-mapped register through an address built from
// constants alone. There is no variable anywhere in the address, so the store
// has no access path; it used to abort the analysis of the whole file.

#include <cstdint>
#include <mutex>

#define REG32(addr) (*(volatile std::uint32_t*)(addr))
#define BASE 0x40100000u
#define OTHER 0x40200000u

namespace mmio_absolute_address {

void write_reg_ok(std::uint32_t v) { REG32(BASE + 0x2004u) = v; }

std::uint32_t read_reg_ok() { return REG32(BASE + 0x2008u); }

void set_bits_ok(std::uint32_t mask) { REG32(BASE + 0x200cu) |= mask; }

// a literal address, with no arithmetic to see through
void write_literal_ok(std::uint32_t v) { REG32(0x40103000u) = v; }

void write_nested_ok(std::uint32_t v) { REG32(BASE + 0x1000u + 0x14u) = v; }

void write_indexed_ok(std::uint32_t i, std::uint32_t v) {
  ((volatile std::uint32_t*)BASE)[i] = v;
}

struct Registers {
  std::uint32_t control;
  std::uint32_t status;
};

void write_field_ok(std::uint32_t v) {
  ((volatile Registers*)BASE)->control = v;
}

std::uint32_t read_field_ok() { return ((volatile Registers*)BASE)->status; }

// The same register from two functions, neither holding a lock. A register is
// device state rather than a field of an object, so this is not reported.
void same_register_write_ok(std::uint32_t v) { REG32(BASE + 0x3000u) = v; }

std::uint32_t same_register_read_ok() { return REG32(BASE + 0x3000u); }

void other_register_write_ok(std::uint32_t v) { REG32(OTHER + 0x3000u) = v; }

// A control, so that the file is not silent for the wrong reason: an ordinary
// race in the same translation unit is still reported.
class Counter {
 public:
  void increment_ok() {
    std::lock_guard<std::mutex> guard(mutex_);
    value_++;
  }
  int read_unlocked_bad() { return value_; }

 private:
  std::mutex mutex_;
  int value_ = 0;
};
} // namespace mmio_absolute_address
