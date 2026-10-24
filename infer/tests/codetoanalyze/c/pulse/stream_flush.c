/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <stdio.h>
#include <stdlib.h>

int may_fail(void);

// writing to a stream and flushing it is the shape of every assert and log
// helper
void log_and_flush(FILE* out) {
  fprintf(out, "boom\n");
  fflush(out);
}

void puts_and_flush(FILE* out) {
  fputs("boom\n", out);
  fflush(out);
}

// a caller that holds a null pointer of its own must not be blamed for the
// stream

void write_then_flush_with_unrelated_null_ok() {
  int* p = NULL;
  if (!may_fail()) {
    log_and_flush(stderr);
  }
  (void)p;
}

void puts_then_flush_with_unrelated_null_ok() {
  int* p = NULL;
  if (!may_fail()) {
    puts_and_flush(stderr);
  }
  (void)p;
}

void write_then_flush_param_with_unrelated_null_ok(FILE* out) {
  int* p = NULL;
  if (!may_fail()) {
    log_and_flush(out);
  }
  (void)p;
}

// passing a null stream to such a helper is still a bug
void write_then_flush_null_stream_bad() { log_and_flush(NULL); }

// and so is a null the helper never saw
void unrelated_null_still_bad() {
  int* p = NULL;
  log_and_flush(stderr);
  *p = 1;
}

// flushing every stream at once stays legitimate
void flush_all_streams_ok() { fflush(NULL); }

// a stream that may be null because the open failed is still reported
void flush_unchecked_fopen_bad() {
  FILE* f = fopen("this_file_doesnt_exist", "r");
  fflush(f);
  fprintf(f, "x");
  fclose(f);
}

// known limitation: nothing has required the stream yet when it is flushed, so
// the null case is still explored and the caller's own null is still blamed for
// it
void flush_then_write(FILE* out) {
  fflush(out);
  fprintf(out, "boom\n");
}

void FP_flush_then_write_with_unrelated_null_ok() {
  int* p = NULL;
  if (!may_fail()) {
    flush_then_write(stderr);
  }
  (void)p;
}
