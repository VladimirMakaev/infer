/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

struct RecordBitfields {
  unsigned first : 3;
  unsigned : 2;
  unsigned : 0;
  unsigned second : 4;
  unsigned : 0;
};

int record_bitfields_ok() {
  RecordBitfields value{};
  value.first = 1;
  value.second = 2;
  return value.first + value.second;
}

int record_bitfields_null_bad() {
  RecordBitfields* value = nullptr;
  return value->second;
}
