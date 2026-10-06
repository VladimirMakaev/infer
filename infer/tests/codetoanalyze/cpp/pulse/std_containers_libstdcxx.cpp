/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Mirrors how libstdc++ declares its containers and their iterators. Do not
// include C++ standard headers.
namespace std {

template <typename _T1, typename _T2>
struct pair {
  _T1 first;
  _T2 second;
};

template <typename _Tp>
struct _List_iterator {
  _List_iterator& operator++();
  _Tp& operator*() const;
  friend bool operator==(const _List_iterator& __x, const _List_iterator& __y);
  friend bool operator!=(const _List_iterator& __x, const _List_iterator& __y);
  void* _M_node;
};

template <typename _Tp>
struct list {
  typedef _List_iterator<_Tp> iterator;
  list();
  ~list();
  iterator begin();
  iterator end();
  iterator erase(iterator __position);
  _Tp& front();
  void clear();
  void* _M_impl;
};

template <typename _Tp>
struct _Rb_tree_iterator {
  _Rb_tree_iterator& operator++();
  _Tp& operator*() const;
  _Tp* operator->() const;
  friend bool operator==(const _Rb_tree_iterator& __x,
                         const _Rb_tree_iterator& __y);
  friend bool operator!=(const _Rb_tree_iterator& __x,
                         const _Rb_tree_iterator& __y);
  void* _M_node;
};

template <typename _Tp>
struct _Rb_tree_const_iterator {
  _Rb_tree_const_iterator& operator++();
  const _Tp& operator*() const;
  friend bool operator==(const _Rb_tree_const_iterator& __x,
                         const _Rb_tree_const_iterator& __y);
  friend bool operator!=(const _Rb_tree_const_iterator& __x,
                         const _Rb_tree_const_iterator& __y);
  void* _M_node;
};

template <typename _Key, typename _Tp>
struct map {
  typedef _Rb_tree_iterator<pair<const _Key, _Tp>> iterator;
  map();
  ~map();
  iterator begin();
  iterator end();
  iterator find(const _Key& __x);
  pair<iterator, bool> insert(const pair<const _Key, _Tp>& __x);
  void* _M_t;
};

template <typename _Key>
struct set {
  typedef _Rb_tree_const_iterator<_Key> iterator;
  set(const set& __x);
  ~set();
  iterator begin() const;
  iterator end() const;
  void* _M_t;
};

namespace __detail {

template <typename _Value, bool _Cache_hash_code>
struct _Node_iterator_base {
  friend bool operator==(const _Node_iterator_base& __x,
                         const _Node_iterator_base& __y);
  friend bool operator!=(const _Node_iterator_base& __x,
                         const _Node_iterator_base& __y);
  void* _M_cur;
};

template <typename _Value, bool __constant_iterators, bool __cache>
struct _Node_iterator : _Node_iterator_base<_Value, __cache> {
  _Node_iterator& operator++();
  _Value* operator->() const;
};

} // namespace __detail

template <typename _Key, typename _Tp>
struct unordered_map {
  typedef __detail::_Node_iterator<pair<const _Key, _Tp>, false, false>
      iterator;
  unordered_map();
  ~unordered_map();
  iterator begin();
  iterator end();
  iterator find(const _Key& __x);
  void* _M_h;
};

template <typename _Tp>
struct _Deque_iterator {
  _Tp& operator*() const;
  void* _M_cur;
};

template <typename _Tp>
struct deque {
  typedef _Deque_iterator<_Tp> iterator;
  ~deque();
  iterator begin();
  void push_back(const _Tp& __x);
  void* _M_impl;
};

} // namespace std

namespace libstdcxx {

int iterate_local_set_bad(const std::set<int>& set) {
  std::set<int> local = set;
  int sum = 0;
  for (std::set<int>::iterator it = local.begin(); it != local.end(); ++it) {
    sum += *it;
  }
  return sum;
}

// issue #1884
void range_for_delete_list_ok(std::list<int*>& list) {
  for (int* p : list) {
    delete p;
  }
}

struct RefCounted {
  void Unref() { delete this; }
};

void range_for_unref_map_ok(std::map<int, RefCounted*>& map) {
  for (auto& entry : map) {
    entry.second->Unref();
  }
}

int range_for_list_then_npe_bad(std::list<int>& list) {
  for (int x : list) {
    (void)x;
  }
  int* p = nullptr;
  return *p;
}

int list_erase_bad(std::list<int>& list) {
  auto it = list.begin();
  list.erase(it);
  return *it;
}

int list_front_after_clear_bad(std::list<int>& list) {
  int& first = list.front();
  list.clear();
  return first;
}

int map_find_empty_bad() {
  std::map<int, int> map;
  return map.find(1)->second;
}

int map_find_compare_end_bad(std::map<int, int>& map) {
  auto it = map.find(1);
  if (it == map.end()) {
    return it->second;
  }
  return 0;
}

int map_insert_find_ok() {
  std::map<int, int> map;
  auto result = map.insert({1, 2});
  if (result.second) {
    return result.first->second;
  }
  return map.find(1)->second;
}

int unordered_map_iterate_ok(std::unordered_map<int, int>& map) {
  int sum = 0;
  for (auto it = map.begin(); it != map.end(); ++it) {
    sum += it->second;
  }
  return sum;
}

int unordered_map_find_empty_bad() {
  std::unordered_map<int, int> map;
  return map.find(1)->second;
}

int unordered_map_find_compare_end_bad(std::unordered_map<int, int>& map) {
  auto it = map.find(1);
  if (it == map.end()) {
    return it->second;
  }
  return 0;
}

int deque_push_back_bad(std::deque<int>& deque) {
  auto it = deque.begin();
  deque.push_back(1);
  return *it;
}

} // namespace libstdcxx
