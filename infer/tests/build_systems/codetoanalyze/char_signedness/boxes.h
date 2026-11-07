/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

template <typename T>
struct Box {
  T value;
  Box<T>* next;
};

template <typename T>
void pong(Box<T>* b);

template <typename T>
void ping(Box<T>* b) {
  if (b->next) {
    pong(b->next);
  }
  b->next = nullptr;
}

template <typename T>
void pong(Box<T>* b) {
  if (b->next) {
    ping(b->next);
  }
  b->next = nullptr;
}

// Suppress implicit instantiation so that each translation unit provides exactly one half of
// the ping/pong cycle. The two units disagree about the signedness of plain `char`, so they
// describe `Box<char>` with different types, but plain `char` has a single mangling, so both
// halves land under one name in the capture database.
extern template void ping<char>(Box<char>*);
extern template void pong<char>(Box<char>*);
