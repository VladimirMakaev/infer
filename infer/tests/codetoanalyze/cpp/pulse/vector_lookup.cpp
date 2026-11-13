/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <algorithm>
#include <string>
#include <vector>

namespace vector_lookup {
struct Record {
  std::string key;
  int value = 0;

  explicit Record(std::string key) : key(std::move(key)) {}
};

Record* lookup(std::vector<Record>& records, const std::string& key) {
#if __cplusplus >= 201402L
  auto it = std::find_if(records.begin(), records.end(),
                         [&key](auto& record) { return record.key == key; });
#else
  auto it = std::find_if(records.begin(), records.end(),
                         [&key](Record& record) { return record.key == key; });
#endif
  return it == records.end() ? nullptr : &*it;
}

struct Registry {
  std::vector<Record> records;

  void add(std::string key) {
    Record record(std::move(key));
    records.push_back(std::move(record));
  }

  Record* find(const std::string& key) { return lookup(records, key); }
};

void registered_ok() {
  Registry r;
  r.add("first");
  r.find("first")->value = 1;
}

void never_registered_bad() {
  Registry r;
  r.find("first")->value = 1;
}

void different_key_bad() {
  Registry r;
  r.add("other");
  r.find("first")->value = 1;
}

// Older elements may lose their contents when the existing reallocation model runs.
void FP_registered_first_of_two_ok() {
  Registry r;
  r.add("first");
  r.add("other");
  r.find("first")->value = 1;
}

void registered_second_of_two_ok() {
  Registry r;
  r.add("first");
  r.add("other");
  r.find("other")->value = 1;
}

void missing_after_two_bad() {
  Registry r;
  r.add("first");
  r.add("other");
  r.find("third")->value = 1;
}

void cleared_bad() {
  Registry r;
  r.add("first");
  r.records.clear();
  r.find("first")->value = 1;
}

void changed_key_bad() {
  Registry r;
  r.add("first");
  r.records.front().key = "other";
  r.find("first")->value = 1;
}

// The opaque source reassignment withholds a content proof for the independent copy too.
void FP_copy_keeps_original_key_ok() {
  std::vector<Record> records;
  Record source("first");
  records.push_back(source);
  source.key = "other";
  lookup(records, "first")->value = 1;
}

void true_predicate_ok() {
  std::vector<int> values;
  values.push_back(7);
  auto it = std::find_if(values.begin(), values.end(), [](int) { return true; });
  int* result = it == values.end() ? nullptr : &*it;
  *result = 1;
}

void false_predicate_bad() {
  std::vector<int> values;
  values.push_back(7);
  auto it = std::find_if(values.begin(), values.end(), [](int) { return false; });
  int* result = it == values.end() ? nullptr : &*it;
  *result = 1;
}

void unknown_predicate_bad(bool matches) {
  std::vector<int> values;
  values.push_back(7);
  auto it = std::find_if(values.begin(), values.end(), [matches](int) { return matches; });
  int* result = it == values.end() ? nullptr : &*it;
  *result = 1;
}

void changed_data_bad() {
  std::string a("first"), b(a);
#if __cplusplus >= 201703L
  a.data()[1] = 'X';
#else
  a[1] = 'X';
#endif
  if (a != b) {
    int* p = nullptr;
    *p = 1;
  }
}

void changed_subscript_bad() {
  std::string a("first"), b(a);
  a[1] = 'X';
  if (a != b) {
    int* p = nullptr;
    *p = 1;
  }
}

void changed_iterator_bad() {
  std::string a("first"), b(a);
  *(a.begin() + 1) = 'X';
  if (a != b) {
    int* p = nullptr;
    *p = 1;
  }
}

void changed_const_iterator_bad() {
  std::string a("first"), b(a);
  const std::string& view = a;
  const_cast<char&>(*(view.cbegin() + 1)) = 'X';
  if (a != b) {
    int* p = nullptr;
    *p = 1;
  }
}

void raw_buffer_versions_bad() {
  char buffer[] = "first";
  std::string a(buffer);
  buffer[1] = 'X';
  std::string b(buffer);
  if (a != b) {
    int* p = nullptr;
    *p = 1;
  }
}

void changed_view_bad() {
  std::string a("first"), b(a);
  const char* view = a.c_str();
  const_cast<char*>(view)[1] = 'X';
  if (a != b) {
    int* p = nullptr;
    *p = 1;
  }
}

void incremented_begin_bad() {
  std::vector<int> values;
  values.push_back(7);
  values.push_back(9);
  auto first = values.begin();
  ++first;
  auto it = std::find_if(first, values.end(), [](int x) { return x == 7; });
  int* result = it == values.end() ? nullptr : &*it;
  *result = 1;
}

void offset_begin_bad() {
  std::vector<int> values;
  values.push_back(7);
  values.push_back(9);
  auto it = std::find_if(values.begin() + 1, values.end(), [](int x) { return x == 7; });
  int* result = it == values.end() ? nullptr : &*it;
  *result = 1;
}

void throwing_predicate_ok() {
  std::vector<int> values;
  values.push_back(7);
  try {
    auto it = std::find_if(values.begin(), values.end(), [](int) -> bool { throw 1; });
    int* result = it == values.end() ? nullptr : &*it;
    *result = 1;
  } catch (int) {
  }
}

int readonly_index_copy_bad() {
  std::string a("first"), b(a);
  return a[1] + b[1];
}

int readonly_index_compare_copy_bad() {
  std::string a("first"), b(a);
  int c = a[1] + b[1];
  return c + (a == b);
}
void swallow() {
  try {
    throw 1;
  } catch (int) {
  }
}
struct CatchingMove {
  int value;
  explicit CatchingMove(int n) : value(n) {}
  CatchingMove(CatchingMove&& other) noexcept : value(other.value) { swallow(); }
};
void nested_catch_bad() {
  std::vector<CatchingMove> v;
  v.push_back(CatchingMove(7));
  int* p = nullptr;
  *p = 1;
}
struct Enclosing {
  CatchingMove member;
  explicit Enclosing(int n) : member(n) {}
};
void implicit_member_catch_bad() {
  std::vector<Enclosing> v;
  v.push_back(Enclosing(7));
  int* p = nullptr;
  *p = 1;
}
struct MoveCopiesString {
  std::string key;
  explicit MoveCopiesString(std::string s) : key(std::move(s)) {}
  MoveCopiesString(MoveCopiesString&& other) noexcept : key(other.key) { swallow(); }
};
void string_member_catch_bad() {
  std::vector<MoveCopiesString> v;
  v.push_back(MoveCopiesString("first"));
  int* p = nullptr;
  *p = 1;
}
struct CopyBox {
  std::string value;
  explicit CopyBox(std::string v) : value(std::move(v)) {}
};

std::string external_text;

CopyBox* external_marker_copy_bad() {
  std::string pattern(external_text.c_str());
  CopyBox* box = new CopyBox("");
  box->value = pattern;
  return box;
}

CopyBox* borrowed_global_copy_ok() { return new CopyBox(external_text); }

CopyBox* borrowed_reference_copy_ok(const std::string& text) { return new CopyBox(text); }
template <class T>
struct ReplaceAllocator : std::allocator<T> {
  using value_type = T;
  template <class U>
  struct rebind {
    using other = ReplaceAllocator<U>;
  };
  ReplaceAllocator() = default;
  template <class U>
  ReplaceAllocator(const ReplaceAllocator<U>&) {}
  template <class U, class... Args>
  void construct(U* p, Args&&...) {
    ::new (static_cast<void*>(p)) U(9);
  }
};
void changed_by_allocator_bad() {
  std::vector<int, ReplaceAllocator<int>> v;
  v.push_back(7);
  auto it = std::find_if(v.begin(), v.end(), [](int x) { return x == 7; });
  int* p = it == v.end() ? nullptr : &*it;
  *p = 1;
}
void touch_string();
template <class T>
struct StringAllocator : std::allocator<T> {
  using value_type = T;
  template <class U>
  struct rebind {
    using other = StringAllocator<U>;
  };
  StringAllocator() = default;
  template <class U>
  StringAllocator(const StringAllocator<U>&) {}
  T* allocate(std::size_t n) {
    touch_string();
    return std::allocator<T>{}.allocate(n);
  }
};
using CustomString = std::basic_string<char, std::char_traits<char>, StringAllocator<char>>;
CustomString* touched_string = nullptr;
void touch_string() {
  if (touched_string) (*touched_string)[0] = 'X';
}
void changed_by_string_allocator_bad() {
  CustomString a("a string long enough to require an allocation rather than small string storage");
  touched_string = &a;
  CustomString b("a string long enough to require an allocation rather than small string storage");
  touched_string = nullptr;
  if (a != b) {
    int* p = nullptr;
    *p = 1;
  }
}
void moved_source_bad() {
  std::string a("first"), b(std::move(a));
  if (a != b) {
    int* p = nullptr;
    *p = 1;
  }
}
void counted_literal_bad() {
  std::string a("first", 3), b("first");
  if (a != b) {
    int* p = nullptr;
    *p = 1;
  }
}
void literal_range_bad() {
  std::string a("first", "first" + 3), b("first");
  if (a != b) {
    int* p = nullptr;
    *p = 1;
  }
}
void substring_bad() {
  std::string a("first"), b(a, 0, 3);
  if (a != b) {
    int* p = nullptr;
    *p = 1;
  }
}
void copied_source_unnecessary_copy_bad() {
  std::string a("first"), b(a);
  if (a != b) {
    int* p = nullptr;
    *p = 1;
  }
}
void const_rvalue_unnecessary_copy_bad() {
  const std::string a("first");
  std::string b(std::move(a));
  if (a != b) {
    int* p = nullptr;
    *p = 1;
  }
}
void touch_other_string();
template <class T>
struct ForeignAllocator : std::allocator<T> {
  using value_type = T;
  template <class U>
  struct rebind {
    using other = ForeignAllocator<U>;
  };
  ForeignAllocator() = default;
  template <class U>
  ForeignAllocator(const ForeignAllocator<U>&) {}
  T* allocate(std::size_t n) {
    touch_other_string();
    return std::allocator<T>{}.allocate(n);
  }
};
using ForeignString = std::basic_string<char, std::char_traits<char>, ForeignAllocator<char>>;
std::string* touched_other_string = nullptr;
void touch_other_string() {
  if (touched_other_string) (*touched_other_string)[0] = 'X';
}
void foreign_allocator_changes_string_bad() {
  std::string a("a string long enough to require an allocation rather than small string storage");
  std::string expected(a);
  touched_other_string = &a;
  ForeignString b("a string long enough to require an allocation rather than small string storage");
  touched_other_string = nullptr;
  if (a != expected) {
    int* p = nullptr;
    *p = 1;
  }
}
}  // namespace vector_lookup
