/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

namespace output_switch {
enum Code { Zero, One, Two };

bool decode(int input, Code* output) {
  if (input < 0) {
    return false;
  }
  if (input == 0) {
    *output = Zero;
  } else if (input == 1) {
    *output = One;
  } else {
    *output = Two;
  }
  return true;
}

int known_output_bad(int input) {
  Code code;
  if (!decode(input, &code)) {
    return -1;
  }
  int result = 3; // dead: successful decode writes only named values
  switch (code) {
    case Zero:
      result = 0;
      break;
    case One:
      result = 1;
      break;
    case Two:
      result = 2;
      break;
  }
  return result;
}

int legal_unrestricted_ok(Code code) {
  int result = 3;
  switch (code) {
    case Zero:
      result = 0;
      break;
    case One:
      result = 1;
      break;
    case Two:
      result = 2;
      break;
  }
  return result;
}

bool unknown_decode(Code*);
int unknown_output_ok() {
  Code code;
  if (!unknown_decode(&code)) {
    return -1;
  }
  int result = 3;
  switch (code) {
    case Zero:
      result = 0;
      break;
    case One:
      result = 1;
      break;
    case Two:
      result = 2;
      break;
  }
  return result;
}

bool partial_decode(bool write, Code* output) {
  if (write) {
    *output = Zero;
  }
  return true;
}
int partial_output_ok(bool write) {
  Code code = static_cast<Code>(3);
  if (!partial_decode(write, &code)) {
    return -1;
  }
  int result = 3;
  switch (code) {
    case Zero:
      result = 0;
      break;
    case One:
      result = 1;
      break;
    case Two:
      result = 2;
      break;
  }
  return result;
}

bool decode_pair(Code* first, Code* second) {
  *first = Zero;
  *second = static_cast<Code>(3);
  return true;
}
int overlapping_outputs_ok() {
  Code code;
  if (!decode_pair(&code, &code)) {
    return -1;
  }
  int result = 3;
  switch (code) {
    case Zero:
      result = 0;
      break;
    case One:
      result = 1;
      break;
    case Two:
      result = 2;
      break;
  }
  return result;
}

struct Reset {
  Code* output;
  ~Reset() { *output = static_cast<Code>(3); }
};
bool decode_then_destroy(Code* output) {
  Reset reset{output};
  *output = Zero;
  return true;
}
int destructor_output_ok() {
  Code code;
  if (!decode_then_destroy(&code)) {
    return -1;
  }
  int result = 3;
  switch (code) {
    case Zero:
      result = 0;
      break;
    case One:
      result = 1;
      break;
    case Two:
      result = 2;
      break;
  }
  return result;
}

void change(Code*);
int changed_after_decode_ok(int input) {
  Code code;
  if (!decode(input, &code)) {
    return -1;
  }
  change(&code);
  int result = 3;
  switch (code) {
    case Zero:
      result = 0;
      break;
    case One:
      result = 1;
      break;
    case Two:
      result = 2;
      break;
  }
  return result;
}
} // namespace output_switch
