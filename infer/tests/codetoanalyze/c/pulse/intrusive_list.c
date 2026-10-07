/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <stdlib.h>
#include <string.h>

struct list_node {
  struct list_node* next;
  struct list_node* prev;
};

struct item {
  int value;
  struct list_node node;
};

#define item_of_node(p) \
  ((struct item*)((char*)(p) - (unsigned long)(&((struct item*)0)->node)))

void clear_items_ok(struct list_node* head) {
  while (head->next != head) {
    struct list_node* pos = head->next;
    head->next = pos->next;
    free(item_of_node(pos));
  }
}

int clear_items_then_null_deref_bad(struct list_node* head) {
  clear_items_ok(head);
  int* p = NULL;
  return *p;
}

void clear_items_lookahead_ok(struct list_node* head) {
  struct list_node *pos, *next;
  for (pos = head->next, next = pos->next; pos != head;
       pos = next, next = pos->next) {
    free(item_of_node(pos));
  }
}

// no exit of the loop above is found: [pos == head] is infeasible once
// [pos->next] was read
int FN_clear_items_lookahead_then_null_deref_bad(struct list_node* head) {
  clear_items_lookahead_ok(head);
  int* p = NULL;
  return *p;
}

int value_of_null_item_bad() { return ((struct item*)0)->value; }

struct named_item {
  char* name;
  struct list_node node;
};

#define named_item_of_node(p)        \
  ((struct named_item*)((char*)(p) - \
                        (unsigned long)(&((struct named_item*)0)->node)))

static void add_tail(struct list_node* node, struct list_node* head) {
  struct list_node* prev = head->prev;
  head->prev = node;
  node->next = head;
  node->prev = prev;
  prev->next = node;
}

// the copies are reachable from [dest] only through the address of their
// [node] field, which does not make what they point to reachable: the names
// are reported as leaked
int FP_copy_named_items_ok(struct list_node* src, struct list_node* dest) {
  struct list_node* pos;
  for (pos = src->next; pos != src; pos = pos->next) {
    const char* name = named_item_of_node(pos)->name;
    struct named_item* copy = malloc(sizeof(struct named_item));
    if (!copy) {
      return -1;
    }
    copy->name = malloc(strlen(name) + 1);
    if (!copy->name) {
      free(copy);
      return -1;
    }
    strcpy(copy->name, name);
    add_tail(&copy->node, dest);
  }
  return 0;
}
