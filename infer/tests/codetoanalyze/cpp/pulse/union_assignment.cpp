/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

namespace union_assignment {

enum class Kind { Null, Number };

int* acquire();
union Value {
  int* number;
  char* text;
  Value() = default;
  Value(Kind k) {
    switch (k) {
      case Kind::Number:
        number = acquire();
        break;
      case Kind::Null:
        number = nullptr;
        break;
    }
  }
};

struct Data {
  Kind kind = Kind::Null;
  Value value = {};
};

// Assigning to the union replaces its contents, so the null that [value] was
// initialised with no longer describes it.
int assign_converted_ok() {
  Data d;
  d.kind = Kind::Number;
  d.value = Kind::Number;
  return *d.value.number;
}

int assign_moved_ok() {
  Data d;
  Value v(Kind::Number);
  d.kind = Kind::Number;
  d.value = static_cast<Value&&>(v);
  return *d.value.number;
}

// Constructing the union directly was never affected; keep it as a control.
int construct_ok() {
  Value v(Kind::Number);
  return *v.number;
}

// A union that is never assigned still holds the null it was initialised with.
int never_assigned_bad() {
  Data d;
  return *d.value.number;
}

// Assigning and then writing a null member must still be reported.
int assign_then_null_member_bad() {
  Data d;
  d.value = Kind::Number;
  d.value.number = nullptr;
  return *d.value.number;
}

} // namespace union_assignment
