/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <algorithm>
#include <atomic>
#include <cstdint>
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
}
