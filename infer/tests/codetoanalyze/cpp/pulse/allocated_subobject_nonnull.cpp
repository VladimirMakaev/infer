/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstdlib>
#include <unistd.h>
#include <vector>

namespace allocated_subobject_nonnull {

struct Cell {
  int value;
};

struct Block {
  Cell cell;
  int sibling;
};

// An allocated object's embedded cell cannot be null, even without any reads of that cell.
void embedded_cell_nonnull_ok() {
  Block* block = static_cast<Block*>(malloc(sizeof(Block)));
  if (!block) {
    return;
  }
  block->sibling = 1;
  Cell* cell = &block->cell;
  if (!cell) {
    int* missing = nullptr;
    *missing = 1;
  }
  free(block);
}

void array_element_nonnull_ok() {
  Cell* cells = static_cast<Cell*>(malloc(2 * sizeof(Cell)));
  if (!cells) {
    return;
  }
  Cell* second = &cells[1];
  if (!second) {
    int* missing = nullptr;
    *missing = 1;
  }
  free(cells);
}

// A pointer value stored in an allocation is not an embedded sub-object address.
void stored_nullable_pointer_bad() {
  struct Wrapper {
    int* pointer;
  };
  Wrapper* wrapper = static_cast<Wrapper*>(malloc(sizeof(Wrapper)));
  if (!wrapper) {
    return;
  }
  wrapper->pointer = nullptr;
  int* pointer = wrapper->pointer;
  free(wrapper);
  *pointer = 1;
}

void nullable_parameter_bad(int* pointer) {
  if (!pointer) {
    *pointer = 1;
  }
}

void null_parameter_call_bad() { nullable_parameter_bad(nullptr); }

// Zero stored in a scalar field remains a possible value.
void zero_field_fd_leak_bad(int source) {
  int fd = dup(source);
  if (fd < 0) {
    return;
  }
  Block* block = new Block;
  block->sibling = 0;
  bool zero = block->sibling == 0;
  delete block;
  if (!zero) {
    close(fd);
  }
}

void vector_element_leak_bad() {
  std::vector<int*> values;
  values.push_back(static_cast<int*>(malloc(sizeof(int))));
}

} // namespace allocated_subobject_nonnull
