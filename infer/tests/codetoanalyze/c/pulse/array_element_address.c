/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <stdlib.h>

void array_addr_fill_unknown(unsigned*);

static void array_addr_write_cell(unsigned* cell) { *cell = 7; }

static void array_addr_write_index(unsigned* cell) { cell[0] = 7; }

static unsigned array_addr_read_cell(const unsigned* cell) { return *cell; }

static unsigned array_addr_noop_cell(unsigned* cell) { return 0; }

static void array_addr_empty_noop(unsigned* cell) { (void)cell; }

void array_addr_zero_ok(void) {
  unsigned* array = malloc(4 * sizeof(unsigned));
  if (!array) return;
  array_addr_fill_unknown(&array[0]);
  free(array);
}

void array_addr_index_ok(unsigned index) {
  unsigned* array = malloc(4 * sizeof(unsigned));
  if (!array) return;
  if (index < 4) array_addr_fill_unknown(&array[index]);
  free(array);
}

void array_addr_direct_ok(void) {
  unsigned* array = malloc(4 * sizeof(unsigned));
  if (!array) return;
  array_addr_fill_unknown(array);
  free(array);
}

void array_addr_arithmetic_ok(void) {
  unsigned* array = malloc(4 * sizeof(unsigned));
  if (!array) return;
  array_addr_fill_unknown(array + 0);
  free(array);
}

void array_addr_local_array_ok(void) {
  unsigned array[4];
  array_addr_fill_unknown(&array[1]);
}

unsigned array_addr_known_fill_ok(void) {
  unsigned* array = malloc(sizeof(unsigned));
  if (!array) return 0;
  array_addr_write_cell(&array[0]);
  unsigned value = array[0];
  free(array);
  return value;
}

unsigned array_addr_known_index_fill_ok(unsigned index) {
  unsigned* array = malloc(4 * sizeof(unsigned));
  if (!array) return 0;
  unsigned value = 0;
  if (index < 4) {
    array_addr_write_cell(&array[index]);
    value = array[index];
  }
  free(array);
  return value;
}

unsigned array_addr_saved_written_ok(void) {
  unsigned* array = malloc(sizeof(unsigned));
  if (!array) return 0;
  unsigned* cell = &array[0];
  array_addr_write_cell(cell);
  unsigned value = *cell;
  free(array);
  return value;
}

static unsigned array_addr_unknown_fill_helper(unsigned* array) {
  array_addr_fill_unknown(&array[0]);
  return array[0];
}

unsigned array_addr_unknown_fill_ok(void) {
  unsigned* array = malloc(sizeof(unsigned));
  if (!array) return 0;
  unsigned value = array_addr_unknown_fill_helper(array);
  free(array);
  return value;
}

unsigned array_addr_unknown_callback_ok(void (*fill)(unsigned*)) {
  unsigned* array = malloc(sizeof(unsigned));
  if (!array) return 0;
  fill(&array[0]);
  unsigned value = array[0];
  free(array);
  return value;
}

unsigned array_addr_read_bad(void) {
  unsigned* array = malloc(sizeof(unsigned));
  if (!array) return 0;
  unsigned value = array[0];
  free(array);
  return value;
}

unsigned array_addr_index_read_bad(unsigned index) {
  unsigned* array = malloc(4 * sizeof(unsigned));
  if (!array) return 0;
  unsigned value = 0;
  if (index < 4) value = array[index];
  free(array);
  return value;
}

unsigned array_addr_saved_read_bad(void) {
  unsigned* array = malloc(sizeof(unsigned));
  if (!array) return 0;
  unsigned* cell = &array[0];
  unsigned value = *cell;
  free(array);
  return value;
}

unsigned array_addr_callback_read_bad(void) {
  unsigned* array = malloc(sizeof(unsigned));
  if (!array) return 0;
  unsigned value = array_addr_read_cell(&array[0]);
  free(array);
  return value;
}

unsigned array_addr_noop_then_read_bad(void) {
  unsigned* array = malloc(sizeof(unsigned));
  if (!array) return 0;
  array_addr_noop_cell(&array[0]);
  unsigned value = array[0];
  free(array);
  return value;
}

// Indexed writes still use the existing coarse allocation-initialization approximation.
unsigned FN_array_addr_write_other_read_bad(void) {
  unsigned* array = malloc(2 * sizeof(unsigned));
  if (!array) return 0;
  array[0] = 7;
  unsigned value = array[1];
  free(array);
  return value;
}

unsigned array_addr_write_then_read_ok(void) {
  unsigned* array = malloc(sizeof(unsigned));
  if (!array) return 0;
  array[0] = 7;
  unsigned value = array[0];
  free(array);
  return value;
}

void array_addr_uninitialized_pointer_bad(void) {
  unsigned* array;
  array_addr_fill_unknown(&array[0]);
}

void array_addr_uninitialized_index_bad(void) {
  unsigned index;
  unsigned* array = malloc(4 * sizeof(unsigned));
  if (!array) return;
  array_addr_fill_unknown(&array[index]);
  free(array);
}

void array_addr_pointer_element_read_bad(void) {
  unsigned** array = malloc(sizeof(unsigned*));
  if (!array) return;
  array_addr_fill_unknown(array[0]);
  free(array);
}

void array_addr_null_bad(void) {
  unsigned* array = NULL;
  array_addr_write_cell(&array[0]);
}

void array_addr_null_index_bad(unsigned index) {
  unsigned* array = NULL;
  array_addr_write_cell(&array[index]);
}

unsigned array_addr_null_read_bad(void) {
  unsigned* array = NULL;
  return array[0];
}

void array_addr_freed_bad(void) {
  unsigned* array = malloc(sizeof(unsigned));
  if (!array) return;
  free(array);
  array_addr_write_cell(&array[0]);
}

unsigned array_addr_freed_read_bad(void) {
  unsigned* array = malloc(sizeof(unsigned));
  if (!array) return 0;
  free(array);
  return array[0];
}

struct array_addr_record {
  unsigned first;
  unsigned second;
};

void array_addr_nested_field_ok(void) {
  struct array_addr_record* records = malloc(2 * sizeof(*records));
  if (!records) return;
  array_addr_write_cell(&records[1].second);
  free(records);
}

void array_addr_nested_array_ok(void) {
  unsigned (*rows)[3] = malloc(2 * sizeof(*rows));
  if (!rows) return;
  array_addr_fill_unknown(&rows[0][1]);
  free(rows);
}

struct array_addr_holder {
  unsigned* array;
  unsigned other;
};

void array_addr_pointer_field_ok(unsigned index) {
  struct array_addr_holder holder;
  holder.array = malloc(4 * sizeof(unsigned));
  if (!holder.array) return;
  if (index < 4) array_addr_fill_unknown(&holder.array[index]);
  free(holder.array);
}

unsigned array_addr_struct_read_bad(void) {
  struct array_addr_record* record = malloc(sizeof(*record));
  if (!record) return 0;
  record->first = 7;
  unsigned value = record->second;
  free(record);
  return value;
}

unsigned array_addr_scalar_index_writer_ok(void) {
  unsigned value;
  array_addr_write_index(&value);
  return value;
}

unsigned array_addr_indexed_writer_ok(void) {
  unsigned* array = malloc(sizeof(unsigned));
  if (!array) return 0;
  array_addr_write_index(&array[0]);
  unsigned value = array[0];
  free(array);
  return value;
}

// This empty-callee approximation already misses the direct-pointer read on the base.
unsigned FN_array_addr_empty_noop_direct_read_bad(void) {
  unsigned* array = malloc(sizeof(unsigned));
  if (!array) return 0;
  array_addr_empty_noop(array);
  unsigned value = *array;
  free(array);
  return value;
}

struct field_addr_payload {
  void (*callback)(void*);
  void* context;
};

struct field_addr_entry {
  void* next;
  struct field_addr_payload payload;
};

static void field_addr_copy_payload(const struct field_addr_payload* source,
                                    void* destination) {
  ((struct field_addr_entry*)destination)->payload = *source;
}

void field_addr_buffer_store_ok(void (*callback)(void*), void* context) {
  struct field_addr_payload source = {callback, context};
  // Computing the nested destination address does not read the uninitialized buffer.
  void* destination = malloc(4 * sizeof(void*));
  if (!destination) return;
  field_addr_copy_payload(&source, destination);
  free(destination);
}

void* field_addr_buffer_source_read_bad(void) {
  struct field_addr_payload* source = malloc(sizeof(*source));
  if (!source) return NULL;
  struct field_addr_entry destination;
  // The source contents really are read by the aggregate assignment.
  field_addr_copy_payload(source, &destination);
  void* value = destination.payload.context;
  free(source);
  return value;
}
