/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

template <typename... Ts>
int count(Ts... ts) {
  return sizeof...(Ts) + sizeof...(ts);
}

int two() { return count(1, 2); }
