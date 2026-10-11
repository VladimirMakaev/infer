/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <string>
#include <utility>
#include <vector>

namespace max_disjuncts {
int get_full_disjs();
}

namespace copy_snapshots {
struct Token {
  std::string text;
  int line;
};
struct Stream {
  Token token;
  const Token& current() { return token; }
};

struct Reader {
  Stream stream;
  int depth;

  int read_after_join_bad(bool branch) {
    (void)stream.current().text;
    if (branch) {
      ++depth;
    } else {
      --depth;
    }
    const std::string snapshot = stream.current().text;
    return snapshot.size();
  }

  int source_changed_after_join_ok(bool branch) {
    (void)stream.current().text;
    if (branch) {
      ++depth;
    } else {
      --depth;
    }
    const std::string snapshot = stream.current().text;
    stream.token.text.clear();
    return snapshot.size();
  }

  int source_changed_on_one_path_ok(bool branch) {
    (void)stream.current().text;
    if (branch) {
      ++depth;
    } else {
      --depth;
    }
    const std::string snapshot = stream.current().text;
    if (branch) {
      stream.token.text.clear();
    }
    return snapshot.size();
  }
};

struct Item {
  std::string name;
  std::vector<int> values;
};
std::vector<Item> after_loop_bad(int count) {
  Item value;
  value.name = "value";
  for (int i = 0; i < count; ++i) {
    value.values.push_back(i);
  }
  return {value};
}

int use_string(const std::string& value) { return value.size(); }
int take_string(std::string value) { return value.size(); }
int repeated_loop_source_ok(int count) {
  std::string value = "value";
  int result = 0;
  for (int i = 0; i < count; ++i) {
    result += take_string(value);
  }
  return result;
}

int source_used_after_same_line_ok() {
  std::string value = "value";
  int result = take_string(value); use_string(value);
  return result;
}

int moved_source_snapshot_ok(std::string& value) {
  const std::string snapshot = value;
  take_string(std::move(value));
  return snapshot.size();
}

struct Document { std::string text; };
Document document();
int temporary_source_snapshot_ok() {
  const std::string snapshot = document().text;
  return snapshot.size();
}

struct ArrayRecord {
  int elements[2];
  std::vector<int> padding;
};

void modify_record(ArrayRecord& record) { record.elements[0] = 42; }

int alias_write_on_dropped_path_ok(const std::vector<int>& values,
                                 const ArrayRecord& source) {
  int paths = max_disjuncts::get_full_disjs();
  ArrayRecord snapshot = source;
  ArrayRecord* alias = &snapshot;
  for (const int& value : values) {
    alias->elements[0] = 42;
  }
  return snapshot.elements[0];
}

int source_write_on_dropped_path_ok(const std::vector<int>& values,
                                  ArrayRecord& source) {
  int paths = max_disjuncts::get_full_disjs();
  const ArrayRecord snapshot = source;
  for (const int& value : values) {
    source.elements[0] = 42;
  }
  return snapshot.elements[0];
}

int callee_write_on_dropped_path_ok(const std::vector<int>& values,
                                  const ArrayRecord& source) {
  int paths = max_disjuncts::get_full_disjs();
  ArrayRecord snapshot = source;
  for (const int& value : values) {
    modify_record(snapshot);
  }
  return snapshot.elements[0];
}

int callee_source_write_on_dropped_path_ok(const std::vector<int>& values,
                                         ArrayRecord& source) {
  int paths = max_disjuncts::get_full_disjs();
  const ArrayRecord snapshot = source;
  for (const int& value : values) {
    modify_record(source);
  }
  return snapshot.elements[0];
}

ArrayRecord* unknown_record();
int representative_alias_write_ok(const ArrayRecord& source) {
  ArrayRecord* alias = unknown_record();
  ArrayRecord snapshot = source;
  if (alias == &snapshot) {
    alias->elements[0] = 42;
  }
  return snapshot.elements[0];
}

int representative_callee_write_ok(const ArrayRecord& source) {
  ArrayRecord* alias = unknown_record();
  ArrayRecord snapshot = source;
  if (alias == &snapshot) {
    modify_record(*alias);
  }
  return snapshot.elements[0];
}
} // namespace copy_snapshots
