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

// ---------------- a matching callee that takes no arguments ----------------
//
// A free with no pointer argument frees nothing, so matching it buys nothing -- but a matched
// model REPLACES the call, so answering with a no-op loses whatever the callee does. These pin
// that a zero-arity callee keeps its body: the pair differs only in which value the branch
// tests, so if the body were dropped both verdicts would flip rather than one of them changing.

namespace hw {
int g_state;
void close_all_sessions() { g_state = 42; }
template <typename T>
void close_all() {
  g_state = 42;
}
} // namespace hw

// unreachable: close_all_sessions set g_state to 42
void zero_arity_body_survives_ok() {
  hw::g_state = 0;
  hw::close_all_sessions();
  if (hw::g_state == 0) {
    int* p = nullptr;
    *p = 1;
  }
}

// reachable, and the positive control for the same shape
void zero_arity_control_bad() {
  hw::g_state = 0;
  hw::close_all_sessions();
  if (hw::g_state == 42) {
    int* p = nullptr;
    *p = 1;
  }
}

// the same pair through a templated callee, which takes the same matcher path
void tmpl_zero_arity_body_survives_ok() {
  hw::g_state = 0;
  hw::close_all<int>();
  if (hw::g_state == 0) {
    int* p = nullptr;
    *p = 1;
  }
}

void tmpl_zero_arity_control_bad() {
  hw::g_state = 0;
  hw::close_all<int>();
  if (hw::g_state == 42) {
    int* p = nullptr;
    *p = 1;
  }
}
