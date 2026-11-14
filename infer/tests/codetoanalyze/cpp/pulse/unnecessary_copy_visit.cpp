/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#if __cplusplus >= 201703L

#include <set>
#include <type_traits>
#include <variant>
#include <vector>

// A snapshot taken because a callee writes through a reference parameter is reported as an
// unnecessary copy when the callee dispatches the write through an `overloaded{...}` visitor --
// the usual idiom of a struct that inherits several lambdas' operator() via using-declarations.
//
// The trigger is that visitor, NOT std::visit. Through std::visit the write stays visible for an
// ordinary struct visitor, for a single generic lambda and for a monomorphic lambda, and reaching
// the same write inline with std::get_if, with no visitor at all, is fine too; all four are below
// as controls and all four are silent. Only the inherited-lambda visitor loses the write. Why it
// does is not established here: this file pins the shape that reports and fences it with the
// neighbouring shapes that do not, so that a fix can be aimed at the right thing and proved
// against it.

namespace unnecessary_copy_visit {

struct AddOp {
  int sig;
};

struct NopOp {
  int unused;
};

using Op = std::variant<AddOp, NopOp>;
using OneOp = std::variant<AddOp>;

template <class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

// `written` is modified here, but the write is dispatched through an `overloaded{...}` visitor.
// That write does not reach this procedure's summary.
void overloaded_visit_writer(const std::vector<Op>& body, std::set<int>& written) {
  for (const auto& op : body) {
    std::visit(overloaded{
                   [&](const AddOp& a) { written.insert(a.sig); },
                   [&](const NopOp&) {},
               },
               op);
  }
}

// The snapshot is required: overloaded_visit_writer modifies the set it is given, so dropping the
// copy would write through to the caller's `written`, which is a visible side effect on a
// by-reference parameter. Pulse reports the copy anyway, because the callee's summary records no
// write to its second parameter.
bool FP_snapshot_across_overloaded_visitor_ok(const std::vector<Op>& body,
                                              std::set<int>& written,
                                              int probe) {
  auto body_w = written;
  overloaded_visit_writer(body, body_w);
  return body_w.count(probe) > 0;
}

// ---------------------------------------------------------------------------
// Controls. Each writes the same parameter in the same loop and is correctly left alone, so the
// row above is pinned to the inherited-lambda visitor rather than to std::visit, to lambdas, or
// to the snapshot shape. All four must stay silent.
// ---------------------------------------------------------------------------

// std::visit with an ordinary struct visitor: no lambda, no inheritance.
struct StructVisitor {
  std::set<int>& out;
  void operator()(const AddOp& a) const { out.insert(a.sig); }
  void operator()(const NopOp&) const {}
};

void struct_visit_writer(const std::vector<Op>& body, std::set<int>& written) {
  for (const auto& op : body) {
    std::visit(StructVisitor{written}, op);
  }
}

bool snapshot_across_struct_visitor_ok(const std::vector<Op>& body,
                                       std::set<int>& written,
                                       int probe) {
  auto body_w = written;
  struct_visit_writer(body, body_w);
  return body_w.count(probe) > 0;
}

// std::visit with a single generic lambda: a lambda, but no `overloaded` and no inheritance.
void generic_lambda_visit_writer(const std::vector<Op>& body, std::set<int>& written) {
  for (const auto& op : body) {
    std::visit(
        [&](auto&& arg) {
          if constexpr (std::is_same_v<std::decay_t<decltype(arg)>, AddOp>) {
            written.insert(arg.sig);
          }
        },
        op);
  }
}

bool snapshot_across_generic_lambda_ok(const std::vector<Op>& body,
                                       std::set<int>& written,
                                       int probe) {
  auto body_w = written;
  generic_lambda_visit_writer(body, body_w);
  return body_w.count(probe) > 0;
}

// std::visit with a monomorphic lambda over a one-alternative variant: the simplest lambda
// through visit there is.
void mono_lambda_visit_writer(const std::vector<OneOp>& body, std::set<int>& written) {
  for (const auto& op : body) {
    std::visit([&](const AddOp& a) { written.insert(a.sig); }, op);
  }
}

bool snapshot_across_mono_lambda_ok(const std::vector<OneOp>& body,
                                    std::set<int>& written,
                                    int probe) {
  auto body_w = written;
  mono_lambda_visit_writer(body, body_w);
  return body_w.count(probe) > 0;
}

// The same write reached with std::get_if, with no visitor and no dispatch at all.
void direct_writer(const std::vector<Op>& body, std::set<int>& written) {
  for (const auto& op : body) {
    if (const AddOp* a = std::get_if<AddOp>(&op)) {
      written.insert(a->sig);
    }
  }
}

bool snapshot_direct_ok(const std::vector<Op>& body,
                        std::set<int>& written,
                        int probe) {
  auto body_w = written;
  direct_writer(body, body_w);
  return body_w.count(probe) > 0;
}

} // namespace unnecessary_copy_visit

#endif
