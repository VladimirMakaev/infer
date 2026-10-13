/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
#include <string>
#include <string_view>

namespace string_view_remove {

// remove_prefix and remove_suffix both require n <= size()

int remove_prefix_past_the_end_bad() {
  std::string owner = "hello";
  std::string_view view = owner;
  view.remove_prefix(99);
  return view[0];
}

int remove_suffix_past_the_end_bad() {
  std::string owner = "hello";
  std::string_view view = owner;
  view.remove_suffix(99);
  return view[0];
}

int remove_prefix_one_too_many_bad() {
  std::string owner = "hello";
  std::string_view view = owner;
  view.remove_prefix(6);
  return view[0];
}

int remove_suffix_one_too_many_bad() {
  std::string owner = "hello";
  std::string_view view = owner;
  view.remove_suffix(6);
  return view[0];
}

int remove_prefix_ok() {
  std::string owner = "hello";
  std::string_view view = owner;
  view.remove_prefix(2);
  return view[0];
}

int remove_suffix_ok() {
  std::string owner = "hello";
  std::string_view view = owner;
  view.remove_suffix(2);
  return view[0];
}

// removing exactly size() is allowed and leaves an empty view
int remove_prefix_everything_ok() {
  std::string owner = "hello";
  std::string_view view = owner;
  view.remove_prefix(5);
  return (int)view.size();
}

int remove_suffix_everything_ok() {
  std::string owner = "hello";
  std::string_view view = owner;
  view.remove_suffix(5);
  return (int)view.size();
}

int remove_nothing_ok() {
  std::string owner = "hello";
  std::string_view view = owner;
  view.remove_prefix(0);
  return view[0];
}

int remove_both_ends_ok() {
  std::string owner = "hello";
  std::string_view view = owner;
  view.remove_prefix(2);
  view.remove_suffix(2);
  return view[0];
}

// the length is not known, so no claim is made about it
int remove_prefix_unknown_amount_ok(unsigned amount) {
  std::string owner = "hello";
  std::string_view view = owner;
  view.remove_prefix(amount);
  return view[0];
}

// the view still borrows the same string, so losing the owner is still reported
int remove_prefix_keeps_the_borrow_bad() {
  std::string_view view;
  {
    std::string owner = "hello";
    view = owner;
    view.remove_prefix(1);
  }
  return view[0];
}

} // namespace string_view_remove
