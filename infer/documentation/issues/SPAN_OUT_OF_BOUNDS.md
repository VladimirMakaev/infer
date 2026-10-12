A subview of a `std::span` leaves the sequence the span covers, so reading or
writing through it accesses memory outside that sequence. `first(count)` and
`last(count)` require `count` to be at most the extent of the span, and
`subspan(offset, count)` requires `offset` to be at most the extent and `count`
to be at most what is left after `offset`; going past either is undefined
behaviour, even though neither function throws.

```cpp
int subspan_out_of_bounds_bad() {
  int a[3] = {1, 2, 3};
  std::span<int> s(a);
  auto t = s.subspan(5); // bad: the span only covers 3 elements
  return t[0];
}

int first_out_of_bounds_bad(std::vector<int>& v) {
  v.resize(2);
  return std::span<int>(v).first(5)[0]; // bad: only 2 elements to take from
}
```

The extent is compared only when it is known: a span built from a pointer and a
count that the analysis cannot pin down makes no bounds claim, so the following
is not reported.

```cpp
int unknown_extent_ok(int* p, std::size_t n) {
  std::span<int> s(p, n);
  return s.subspan(4)[0]; // not reported: `n` is unknown, so `4` may be in range
}
```

Element access through `operator[]`, `front()` and `back()` is not bounds
checked, as for `std::vector`.

A span that dangles because the sequence it views was destroyed or reallocated
is reported as
[`VECTOR_INVALIDATION`](#vector_invalidation) instead, together with the other
ways of holding on to a container's storage for too long.
