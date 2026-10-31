/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <memory>
#include <system_error>

int sink(int);

// --- the boolean conversion itself, both directions ----------------------------------------
int error_code_from_errc_is_set_ok() {
  std::error_code code = std::make_error_code(std::errc::invalid_argument);
  if (!code) {
    int* p = nullptr;
    return sink(*p); // unreachable: a nonzero code converts to true
  }
  return 0;
}

int default_error_code_is_clear_ok() {
  std::error_code code = {};
  if (code) {
    int* p = nullptr;
    return sink(*p); // unreachable: a default code converts to false
  }
  return 0;
}

int cleared_error_code_is_clear_ok() {
  std::error_code code = std::make_error_code(std::errc::invalid_argument);
  code.clear();
  if (code) {
    int* p = nullptr;
    return sink(*p); // unreachable after clear()
  }
  return 0;
}

int copied_error_code_keeps_its_value_ok() {
  std::error_code code = std::make_error_code(std::errc::invalid_argument);
  std::error_code copy = code;
  if (!copy) {
    int* p = nullptr;
    return sink(*p); // unreachable: the copy carries the same value
  }
  return 0;
}

int assigned_error_code_keeps_its_value_ok() {
  std::error_code code;
  code = std::make_error_code(std::errc::invalid_argument);
  if (!code) {
    int* p = nullptr;
    return sink(*p); // unreachable: the assignment carries the value
  }
  return 0;
}

// --- a status object whose predicate bottoms out in the code, as such types do --------------
struct Status {
  std::error_code _code = {};
  std::shared_ptr<const std::exception> _exception = {};
  Status() = default;
  static Status failure() {
    Status s;
    s._code = std::make_error_code(std::errc::invalid_argument);
    return s;
  }
  operator bool() const { return _code.operator bool() || _exception != nullptr; }
};

struct Thing {
  int field = 0;
};
struct Registry {
  std::shared_ptr<Thing> find(int id) const;
};
Registry const& registry();

// the status is set on exactly the branch that returns null
static std::shared_ptr<Thing> get(Status& status, int id) {
  status = {};
  auto found = registry().find(id);
  if (found == nullptr) {
    status = Status::failure();
    return nullptr;
  }
  return found;
}

int checked_status_then_use_ok(int id) {
  Status status;
  auto thing = get(status, id);
  if (status) {
    return 0;
  }
  return sink(thing->field);
}

int checked_status_in_lambda_ok(int id) {
  auto body = [&](Status& status) {
    auto thing = get(status, id);
    if (status) {
      return 0;
    }
    return sink(thing->field);
  };
  Status status;
  return body(status);
}

int checked_status_twice_ok(int id) {
  Status status;
  auto first = get(status, id);
  if (status) {
    return 0;
  }
  auto second = get(status, first->field);
  if (status) {
    return 0;
  }
  return sink(second->field);
}

// --- true positives: these must keep reporting ----------------------------------------------
int unchecked_status_then_use_bad(int id) {
  Status status;
  auto thing = get(status, id);
  return sink(thing->field); // the status is never looked at
}

int status_checked_with_wrong_polarity_bad(int id) {
  Status status;
  auto thing = get(status, id);
  if (!status) {
    return 0;
  }
  return sink(thing->field); // reached only on the failure path, where the pointer is null
}
