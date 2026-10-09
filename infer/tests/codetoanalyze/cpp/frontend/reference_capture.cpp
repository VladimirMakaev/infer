/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

namespace reference_capture {

struct Cell {
  int value;
};

void rvalue_reference(Cell&& cell) {
  auto update = [&cell] { cell.value = 7; };
  update();
}

void nested_rvalue_reference(Cell&& cell) {
  auto outer = [&cell] {
    auto inner = [&cell] { cell.value = 7; };
    inner();
  };
  outer();
}

void lvalue_reference(Cell& cell) {
  auto update = [&cell] { cell.value = 7; };
  update();
}

void value_capture(Cell cell) {
  auto read = [cell] { return cell.value; };
  read();
}

} // namespace reference_capture
