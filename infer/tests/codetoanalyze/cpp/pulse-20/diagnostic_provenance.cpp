/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>
#if __cplusplus >= 202002L
#include <ranges>
#endif

namespace diagnostic_provenance {
struct Row {
  std::vector<int> targets;
  int key;
};

int iterator_star_bad(const std::vector<Row>& rows) {
  if (rows.empty())
    return 0;
  auto entry = rows.begin();
  auto targets = (*entry).targets;
  return targets.size();
}

int iterator_arrow_bad(const std::vector<Row>& rows) {
  if (rows.empty())
    return 0;
  auto entry = rows.begin();
  auto targets = entry->targets;
  return targets.size();
}

int reference_iterator_bad(std::vector<Row>::const_iterator& entry) {
  auto targets = entry->targets;
  return targets.size();
}

int pointer_iterator_bad(std::vector<Row>::const_iterator* entry) {
  auto targets = (*entry)->targets;
  return targets.size();
}

int find_iterator_bad(const std::vector<Row>& rows) {
  auto entry = std::find_if(rows.begin(), rows.end(), [](const Row&) {
    return true;
  });
  if (entry == rows.end())
    return 0;
  auto targets = entry->targets;
  return targets.size();
}

#if __cplusplus >= 202002L
int ranges_find_bad(const std::vector<Row>& rows, int key) {
  auto entry = std::ranges::find(rows, key, &Row::key);
  if (entry == rows.end())
    return 0;
  auto targets = entry->targets;
  return targets.size();
}
#endif

int raw_array_bad(const Row* rows, int index) {
  auto targets = rows[index].targets;
  return targets.size();
}

int arithmetic_index_bad(const Row* rows, int index) {
  auto targets = rows[index + 1].targets;
  return targets.size();
}

struct Config {
  std::vector<int> __fbthrift_field_path;
  std::vector<int> path;
  std::vector<int> user__fbthrift_field_path;
  std::vector<int> __fbthrift_field_;
  std::vector<int> __fbthrift_field_9path;
  std::vector<int> __fbthrift_field_field_path_deadbeef;
};

int generated_field_bad(const Config* cfg) {
  auto path = cfg->__fbthrift_field_path;
  return path.size();
}

int ordinary_field_bad(const Config* cfg) {
  auto path = cfg->path;
  return path.size();
}

int embedded_prefix_bad(const Config* cfg) {
  auto path = cfg->user__fbthrift_field_path;
  return path.size();
}

int empty_suffix_bad(const Config* cfg) {
  auto path = cfg->__fbthrift_field_;
  return path.size();
}

int invalid_suffix_bad(const Config* cfg) {
  auto path = cfg->__fbthrift_field_9path;
  return path.size();
}

int generated_suffix_bad(const Config* cfg) {
  auto path = cfg->__fbthrift_field_field_path_deadbeef;
  return path.size();
}

struct Proxy {
  const Config* cfg;
  const Config* operator->() const { return cfg; }
};

int arrow_operator_bad(const Config* cfg) {
  Proxy proxy{cfg};
  auto path = proxy->path;
  return path.size();
}

int captured_field_bad(const Config* cfg) {
  auto callback = [cfg] {
    auto path = cfg->path;
    return path.size();
  };
  return callback();
}

int captured_name_bad(const std::vector<int>& __fbthrift_field_path) {
  auto callback = [&__fbthrift_field_path] {
    auto path = __fbthrift_field_path;
    return path.size();
  };
  return callback();
}

// Preserve raw indexing even when the base and index print identically.
int raw_identical_bad(const Row* entry) {
  auto targets = entry[reinterpret_cast<std::uintptr_t>(entry)].targets;
  return targets.size();
}

int map_iterator_bad(const std::map<int, Row>& rows) {
  auto entry = rows.begin();
  auto targets = entry->second.targets;
  return targets.size();
}

int map_iterator_star_bad(const std::map<int, Row>& rows) {
  auto entry = rows.begin();
  auto targets = (*entry).second.targets;
  return targets.size();
}

int unordered_map_iterator_bad(const std::unordered_map<int, Row>& rows,
                               int key) {
  auto entry = rows.find(key);
  auto targets = entry->second.targets;
  return targets.size();
}

int pointer_map_iterator_bad(std::map<int, Row>::iterator* entry) {
  auto targets = (*entry)->second.targets;
  return targets.size();
}

int range_for_bad(const std::vector<Row>& rows) {
  int size = 0;
  for (auto row : rows) {
    size += row.targets.size();
  }
  return size;
}

int range_for_ok(const std::vector<Row>& rows) {
  int size = 0;
  for (const auto& row : rows) {
    size += row.targets.size();
  }
  return size;
}

int size_index_bad(const Row* rows, const std::vector<int>& keys) {
  auto targets = rows[keys.size()].targets;
  return targets.size();
}

int use_count_index_bad(const Row* rows, const std::shared_ptr<Row>& owner) {
  auto targets = rows[owner.use_count()].targets;
  return targets.size();
}

int atomic_index_bad(const Row* rows, std::atomic<int>* key) {
  auto targets = rows[key->load()].targets;
  return targets.size();
}

struct Ops {
  void (*release)(int*);
};

const Ops kNullOps{nullptr};

void global_initializer_bad(int* value) { kNullOps.release(value); }

// A null pointer and every zero of the same path are one abstract value, so
// each expression that evaluates to zero competes for its name. Name the
// pointer, not whichever zero was read last.
struct Buffer {
  char* data;
  Buffer() : data(nullptr) {}
};

Buffer make_buffer() { return Buffer(); }

void null_from_callee_with_size_argument_bad(const std::vector<int>& keys) {
  Buffer buffer = make_buffer();
  if (keys.size() == 0) {
    std::memcpy(buffer.data, keys.data(), keys.size());
  }
}

void null_from_callee_with_zero_local_bad(const std::vector<int>& keys) {
  Buffer buffer = make_buffer();
  if (keys.size() == 0) {
    std::size_t size = keys.size();
    std::memcpy(buffer.data, keys.data(), size);
  }
}

void local_null_with_size_argument_bad(const std::vector<int>& keys) {
  char* data = nullptr;
  if (keys.size() == 0) {
    std::memcpy(data, keys.data(), keys.size());
  }
}

// Nothing competes for the name here: a control for the three above rather than
// a case of its own.
void null_from_callee_alone_bad() {
  Buffer buffer = make_buffer();
  std::memcpy(buffer.data, "", 1);
}

// The variables a range-based `for` expands to are not in the source, so name
// the range the loop runs over. Its element is still named after the hidden
// iterator: see range_for_bad above.
struct Span {
  const Row* first;
  const Row* last;
  const Row* begin() const { return first; }
  const Row* end() const { return last; }
};

int range_for_over_null_range_bad() {
  Span* span = nullptr;
  int size = 0;
  for (const auto& row : *span) {
    size += row.targets.size();
  }
  return size;
}

// Known limitation: where the dereference happens while applying a callee's
// summary rather than in a model, the name still comes from the shared value,
// so the later argument wins and this reports `keys->size()`. The pointer is
// named only where the check knows where the value was loaded from.
void copy_into(char* data, std::size_t size) { *data = (char)size; }

void name_of_null_crossing_a_call_bad(const std::vector<int>& keys) {
  Buffer buffer = make_buffer();
  if (keys.size() == 0) {
    copy_into(buffer.data, keys.size());
  }
}
}
