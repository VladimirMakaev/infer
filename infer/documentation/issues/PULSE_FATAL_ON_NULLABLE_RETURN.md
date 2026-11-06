A function that can return null to signal failure was called, and when it does return null the
program deliberately terminates: a `CHECK`, `INVARIANT` or `assert` on the returned value calls
`abort`, a `[[noreturn]]` fatal logger, or any other callee that does not return. The null return
is a documented outcome rather than a bug, so the crash is a reachable one.

This is reported only when the failure is what decides the crash. If the null value is checked and
handled, if the fatal is reached regardless of the value, or if the crash is about something else
entirely, there is no report.

```cpp
// `getRhi()` is on the nullable-return list: it returns null when the device is unavailable.
Rhi* rhi = provider->getRhi();
CHECK_NOTNULL(rhi);  // the program aborts here whenever the device is unavailable
return rhi->width;
```

The fix is to handle the failure rather than assert it away:

```cpp
Rhi* rhi = provider->getRhi();
if (rhi == nullptr) {
  return kDefaultWidth;  // or propagate the failure to a caller that can act on it
}
return rhi->width;
```

This issue type is disabled by default; enable it with
`--enable-issue-type PULSE_FATAL_ON_NULLABLE_RETURN`.
