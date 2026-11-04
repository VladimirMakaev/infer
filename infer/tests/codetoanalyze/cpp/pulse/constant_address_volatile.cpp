/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// CONSTANT_ADDRESS_DEREFERENCE on memory-mapped register overlays. A `volatile`-qualified access
// is the language's own statement that the memory is not ordinary program memory, and addressing
// a device by a fixed address is exactly how it is meant to be reached -- so a constant address
// is not evidence of a bug there.
//
// The suppression is scoped to NON-ZERO constants. A null dereference through a volatile pointer
// is a bug like any other and must keep reporting; the controls below pin that.

namespace constant_address_volatile {

struct Regs {
  volatile unsigned ctrl;
  volatile unsigned status;
};

struct Block {
  Regs regs;
  volatile unsigned flag;
};

struct PlainRegs {
  unsigned ctrl;
  unsigned status;
};

// ---------------- the family being fixed: these must be SILENT ----------------

// the reported shape: a struct overlay at a fixed address, volatile fields
unsigned volatile_struct_read_ok() {
  Regs* r = reinterpret_cast<Regs*>(0x60000000u);
  return r->status;
}

void volatile_struct_write_ok() {
  Regs* r = reinterpret_cast<Regs*>(0x60000000u);
  r->ctrl = 1;
}

// the whole addressing chain of a nested register belongs to reaching it
unsigned volatile_nested_read_ok() {
  Block* b = reinterpret_cast<Block*>(0x60000100u);
  return b->regs.status;
}

void volatile_nested_write_ok() {
  Block* b = reinterpret_cast<Block*>(0x60000100u);
  b->regs.ctrl = 1;
}

// a volatile scalar register written through a bare literal
void volatile_scalar_write_ok() {
  *reinterpret_cast<volatile unsigned*>(0x60000008u) = 7;
}

// an array of registers at a fixed address
void volatile_array_write_ok() {
  volatile unsigned* bank = reinterpret_cast<volatile unsigned*>(0x60000200u);
  bank[2] = 3;
}

// ---------------- controls that MUST keep reporting ----------------

// the same overlay WITHOUT volatile: not a register, still reported
unsigned nonvolatile_struct_read_bad() {
  PlainRegs* r = reinterpret_cast<PlainRegs*>(0x60000000u);
  return r->status;
}

void nonvolatile_struct_write_bad() {
  PlainRegs* r = reinterpret_cast<PlainRegs*>(0x60000000u);
  r->ctrl = 1;
}

// a plain small constant address, no volatile anywhere
int nonvolatile_small_constant_bad() {
  int* p = reinterpret_cast<int*>(4);
  return *p;
}

// THE control for the zero-scoping: a null dereference through a VOLATILE pointer is still a
// null dereference. If this goes silent the suppression has eaten NULLPTR_DEREFERENCE.
unsigned volatile_null_deref_bad() {
  volatile unsigned* p = nullptr;
  return *p;
}

void volatile_null_store_bad() {
  volatile unsigned* p = nullptr;
  *p = 1;
}

// same, through a volatile struct overlay at address 0
unsigned volatile_struct_null_deref_bad() {
  Regs* r = reinterpret_cast<Regs*>(0);
  return r->status;
}

void volatile_struct_null_store_bad() {
  Regs* r = reinterpret_cast<Regs*>(0);
  r->ctrl = 1;
}

// ---------------- the documented, owner-accepted cost of option O1 ----------------

// A volatile write at a tiny address looks like a bug, but with no magnitude gate `volatile`
// alone decides, so this goes silent. The owner accepted that when approving O1, because a
// small literal address is not in fact anomalous on a microcontroller: on Cortex-M, address
// 0x4 is the reset vector, and such registers are reached exactly this way. Gating on
// magnitude -- option O2 -- would keep this report only by bringing those false positives
// back, so the magnitude of the address is not evidence either way and only `volatile` is.
void FN_volatile_small_constant_bad() {
  *reinterpret_cast<volatile int*>(4) = 1;
}

// ---------------- the remaining gap: a volatile READ through a bare pointer ----------------

// C++ strips cv-qualifiers from the prvalue of a non-class lvalue-to-rvalue conversion, so the
// SIL `Load` type of a volatile read is unqualified -- `n$0=*1610612740:unsigned int`. For a
// field the qualifier is recovered from the field's declared type in the type environment, but a
// dereference of a bare pointer keeps no pointer type in SIL at all, so there is nothing left to
// read the volatility from. Writes are unaffected: a `Store`'s type is the lvalue's own type and
// still says `unsigned int volatile`, which is why `volatile_scalar_write_ok` above is silent.
// Fixing this needs the clang frontend to carry the lvalue type on `Load`, which is a wider
// change than option O1 and is not attempted here.
unsigned FP_volatile_scalar_read_ok() {
  return *reinterpret_cast<volatile unsigned*>(0x60000004u);
}

} // namespace constant_address_volatile
