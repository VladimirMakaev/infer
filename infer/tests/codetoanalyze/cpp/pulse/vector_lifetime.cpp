/* Copyright (c) Facebook, Inc. and its affiliates.
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree. */
#include <vector>
#include <memory>
#include <utility>

namespace vector_lifetime_model {
void pop_in_helper(std::vector<int>& v) { v.pop_back(); }
int pop_last_bad() {
  std::vector<int> v;
  v.reserve(2);
  v.push_back(7);
  v.push_back(9);
  int* p = &v.back();
  v.pop_back();
  return *p;
}
int pop_first_ok() {
  std::vector<int> v;
  v.reserve(2);
  v.push_back(7);
  v.push_back(9);
  int* p = &v.front();
  v.pop_back();
  return *p;
}
int pop_first_helper_ok() {
  std::vector<int> v;
  v.reserve(2);
  v.push_back(7);
  v.push_back(9);
  int* p = &v.front();
  pop_in_helper(v);
  return *p;
}
int pop_last_helper_bad() {
  std::vector<int> v;
  v.push_back(7);
  int* p = &v[0];
  pop_in_helper(v);
  return *p;
}
int pop_reuse_ok() {
  std::vector<int> v;
  v.reserve(1);
  v.push_back(7);
  v.pop_back();
  v.push_back(9);
  return v.back();
}
int pop_reuse_old_alias_bad() {
  std::vector<int> v;
  v.reserve(1);
  v.push_back(7);
  int* p = &v[0];
  v.pop_back();
  v.push_back(9);
  return *p;
}
int pop_iterator_bad() {
  std::vector<int> v;
  v.push_back(7);
  auto it = v.begin();
  v.pop_back();
  return *it;
}
int destroy_element_bad() {
  auto* v = new std::vector<int>;
  v->push_back(7);
  int* p = &(*v)[0];
  delete v;
  return *p;
}
int destroy_scope_bad() {
  int* p;
  {
    std::vector<int> v;
    v.push_back(7);
    p = &v[0];
  }
  return *p;
}
int raw_pointee_destroy_ok() {
  int n = 7;
  auto* v = new std::vector<int*>;
  v->push_back(&n);
  int* p = (*v)[0];
  delete v;
  return *p;
}
int copy_destroy_ok() {
  std::vector<int> v;
  v.push_back(7);
  int* p = &v[0];
  { std::vector<int> copy(v); copy[0] = 9; }
  return *p;
}
int move_destroy_source_ok() {
  std::vector<int> dst;
  int* p;
  {
    std::vector<int> src;
    src.push_back(7);
    p = &src[0];
    dst = std::move(src);
  }
  return *p;
}
int move_construct_source_ok() {
  auto* src = new std::vector<int>;
  src->push_back(7);
  int* p = &(*src)[0];
  std::vector<int> dst(std::move(*src));
  delete src;
  return *p;
}
int move_data_destroy_bad() {
  std::vector<int> src;
  src.push_back(7);
  int* p = src.data();
  { std::vector<int> dst(std::move(src)); }
  return *p;
}
int owned_clear_bad() {
  std::vector<std::unique_ptr<int>> v;
  v.push_back(std::unique_ptr<int>(new int(7)));
  int* p = v[0].get();
  v.clear();
  return *p;
}
int owned_live_ok() {
  std::vector<std::unique_ptr<int>> v;
  v.push_back(std::unique_ptr<int>(new int(7)));
  return *v[0];
}
int owned_release_ok() {
  std::vector<std::unique_ptr<int>> v;
  v.push_back(std::unique_ptr<int>(new int(7)));
  int* p = v[0].release();
  v.clear();
  int n = *p;
  delete p;
  return n;
}
void destroy_in_helper(std::vector<int>* v) { delete v; }
int destroy_helper_bad() {
  auto* v = new std::vector<int>;
  v->push_back(7);
  int* p = &(*v)[0];
  destroy_in_helper(v);
  return *p;
}
struct Point { int value; };
void destroy_points_in_helper(std::vector<Point>* v) { delete v; }
int destroy_field_helper_bad() {
  auto* v = new std::vector<Point>;
  v->push_back(Point{7});
  int* p = &(*v)[0].value;
  destroy_points_in_helper(v);
  return *p;
}
int owned_pop_bad() {
  std::vector<std::unique_ptr<int>> v;
  v.push_back(std::unique_ptr<int>(new int(7)));
  int* p = v[0].get();
  v.pop_back();
  return *p;
}
int owned_pop_first_ok() {
  std::vector<std::unique_ptr<int>> v;
  v.reserve(2);
  v.push_back(std::unique_ptr<int>(new int(7)));
  v.push_back(std::unique_ptr<int>(new int(9)));
  int* p = v[0].get();
  v.pop_back();
  return *p;
}
int owned_destroy_bad() {
  auto* v = new std::vector<std::unique_ptr<int>>;
  v->push_back(std::unique_ptr<int>(new int(7)));
  int* p = (*v)[0].get();
  delete v;
  return *p;
}
int shared_clear_observer_ok() {
  auto owner = std::make_shared<int>(7);
  std::vector<std::shared_ptr<int>> v;
  v.push_back(owner);
  int* p = owner.get();
  v.clear();
  return *p;
}
struct CountedOwner {
  int* p;
  static int destroyed;
  explicit CountedOwner(int n) : p(new int(n)) {}
  CountedOwner(const CountedOwner&) = delete;
  CountedOwner(CountedOwner&& other) noexcept : p(other.p) { other.p = nullptr; }
  ~CountedOwner() { if (p) { ++destroyed; delete p; } }
};
int CountedOwner::destroyed = 0;
int owning_class_clear_bad() {
  std::vector<CountedOwner> v;
  v.push_back(CountedOwner(7));
  int* p = v[0].p;
  v.clear();
  return *p;
}
int destructor_count_and_pop_ok() {
  CountedOwner::destroyed = 0;
  std::vector<CountedOwner> v;
  v.reserve(2);
  v.push_back(CountedOwner(7));
  v.push_back(CountedOwner(9));
  v.pop_back();
  v.clear();
  if (CountedOwner::destroyed != 2) {
    int* p = nullptr;
    return *p;
  }
  return 0;
}
struct Observer {
  int* p;
  explicit Observer(int* value) : p(value) {}
  ~Observer() {}
};
int observing_class_clear_ok() {
  int value = 7;
  std::vector<Observer> v;
  v.push_back(Observer(&value));
  int* p = v[0].p;
  v.clear();
  return *p;
}
} // namespace vector_lifetime_model
