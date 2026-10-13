`std::string_view::remove_prefix(n)` and `std::string_view::remove_suffix(n)` both require
`n` to be at most `size()`. They are not checked and they do not throw: the view is simply
moved or shrunk, and because `size()` is unsigned, taking away more than there is wraps it
around to a huge value. Reading through the view afterwards accesses memory outside the
sequence it was viewing.

```cpp
int remove_prefix_bad() {
  std::string s = "hello";
  std::string_view v = s;
  v.remove_prefix(99); // bad: the view only covers 5 characters
  return v[0];
}
```

Removing exactly `size()` characters is fine and leaves an empty view.

```cpp
int remove_prefix_all_ok() {
  std::string s = "hello";
  std::string_view v = s;
  v.remove_prefix(5); // the view is now empty, which is allowed
  return v.size();
}
```

The sizes are compared only when both are known: a view whose extent the analysis cannot
pin down makes no claim, so a `remove_prefix` on it is not reported.

The same kind of mistake on a `std::span` is reported as
[`SPAN_OUT_OF_BOUNDS`](#span_out_of_bounds).
