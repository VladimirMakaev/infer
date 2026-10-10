/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <variant>
#include <vector>

namespace visitor_capture {

template <class... Fs>
struct Overloaded : Fs... {
  using Fs::operator()...;
};
template <class... Fs>
Overloaded(Fs...) -> Overloaded<Fs...>;

void visitor_capture_ok(const std::vector<std::variant<int, float>>& values,
                        int& out) {
  auto rec = [&](auto x) { out += int(x); };
  for (const auto& value : values) {
    std::visit(Overloaded{[&](int x) { rec(x); }, [&](float x) { rec(x); }},
               value);
  }
}

void unused_lambda_in_visitor_bad(
    const std::vector<std::variant<int, float>>& values, int& out) {
  auto unused = [&](auto x) { out += int(x); };
  for (const auto& value : values) {
    std::visit(
        Overloaded{[&](int x) { out += x; }, [&](float x) { out += int(x); }},
        value);
  }
}

void direct_use_ok(const std::variant<int, float>& value, int& out) {
  auto rec = [&](auto x) { out += int(x); };
  std::visit(Overloaded{[&](int x) { rec(x); }, [&](float x) { rec(x); }},
             value);
}

void multiple_alternatives_ok(
    const std::vector<std::variant<int, float, double, char>>& values,
    int& out) {
  auto rec = [&](auto x) { out += int(x); };
  for (const auto& value : values) {
    std::visit(Overloaded{[&](int x) { rec(x); },
                          [&](float x) { rec(x); },
                          [&](double x) { rec(x); },
                          [&](char x) { rec(x); }},
               value);
  }
}

void nested_visitor_ok(const std::vector<std::variant<int, float>>& values,
                       const std::variant<int, float>& inner,
                       int& out) {
  auto rec = [&](auto x) { out += int(x); };
  for (const auto& value : values) {
    std::visit(Overloaded{[&](int x) {
                            std::visit(
                                Overloaded{[&](int y) { rec(x + y); },
                                           [&](float y) { rec(x + int(y)); }},
                                inner);
                          },
                          [&](float x) { rec(x); }},
               value);
  }
}

void nested_unused_bad(const std::vector<std::variant<int, float>>& values,
                       const std::variant<int, float>& inner,
                       int& out) {
  auto unused = [&](auto x) { out += int(x); };
  for (const auto& value : values) {
    std::visit(Overloaded{[&](int x) {
                            std::visit(
                                Overloaded{[&](int y) { out += x + y; },
                                           [&](float y) { out += x + int(y); }},
                                inner);
                          },
                          [&](float x) { out += int(x); }},
               value);
  }
}

} // namespace visitor_capture
