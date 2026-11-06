// The custom alloc/free patterns are regexes over the whole procedure name, but the matcher
// they are registered with used to accept a global name only: a namespaced callee could never
// match, however the pattern was written. These pin both halves -- the namespaced callee is
// matched now, and the global one that always worked still does.

#include <stdlib.h>

namespace hw {
struct Session;
Session* open_session(int id);
void close_session(Session* s);
} // namespace hw

// the allocation is never released
void ns_leak_bad(int id) {
  hw::Session* s = hw::open_session(id);
  if (s) {
  }
}

// released through the free pattern, also namespaced
void ns_released_ok(int id) {
  hw::Session* s = hw::open_session(id);
  hw::close_session(s);
}

// ownership passes to the caller, so this is not a leak here
hw::Session* ns_returns_ok(int id) { return hw::open_session(id); }

// the caller of that wrapper keeps the allocation and drops it
void ns_wrapper_leak_bad(int id) {
  hw::Session* s = ns_returns_ok(id);
  if (s) {
  }
}

// TP control: a plain malloc leak, found with no pattern at all. If this one ever goes quiet
// the run proved nothing about the patterns.
void plain_malloc_leak_bad(void) {
  int* p = (int*)malloc(sizeof(int));
  if (p) {
    *p = 1;
  }
}

// control: the same plain allocation, released
void plain_malloc_ok(void) {
  int* p = (int*)malloc(sizeof(int));
  free(p);
}
