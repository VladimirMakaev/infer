/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Routes for the contract suite: each case is one procedure whose diagnostic
// absence is the property a landed false-positive fix established. The guard
// around each call keeps a path on which the call does not happen, so the
// procedure still has a summary to observe.

#ifndef CONTRACT_CASE
#define CONTRACT_CASE 0
#endif

namespace landing_contract_routes {

int may_fail();

#if CONTRACT_CASE == 0 || CONTRACT_CASE == 1
struct Resource {
  int value;
  int use() { return value; }
};

// Declared and never defined, so the call below is unresolved and may write
// through the reference it is given.
void create(Resource*& out);

void fill_and_use(Resource*& r) {
  create(r);
  r->use();
}

void unknown_outparam_ok() {
  Resource* r = nullptr;
  if (!may_fail()) {
    // @call unknown_outparam_call
    // @operation unknown_outparam_effect
    fill_and_use(r);
  }
}
#endif

#if CONTRACT_CASE == 0 || CONTRACT_CASE == 2
struct FileHandle;
typedef struct FileHandle FILE;
extern "C" int fprintf(FILE*, const char*, ...);
extern "C" int fflush(FILE*);

void log_and_flush(FILE* out) {
  fprintf(out, "boom\n");
  fflush(out);
}

void flush_keeps_validity_ok(FILE* out) {
  int* unrelated = nullptr;
  if (!may_fail()) {
    // @call flush_stream_call
    // @operation flush_stream_effect
    log_and_flush(out);
  }
  (void)unrelated;
}
#endif

} // namespace landing_contract_routes
