/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Bodies of OpenMP regions used to be dropped by the frontend, so every finding inside one was
// lost and the region read as a statement that does nothing -- which also produced wrong
// findings after it. Each case below puts a known true positive inside one directive; the
// no_directive_* row is the same code with no pragma, as a control.

#include <stdlib.h>

void sink(int);


void no_directive_null_deref_bad(void) {
  {
    int* p = nullptr;
    *p = 1;
  }
}

void no_directive_use_after_free_bad(void) {
  {
    int* p = (int*)malloc(sizeof(int));
    if (p) {
      free(p);
      *p = 1;
    }
  }
}

void no_directive_leak_bad(void) {
  {
    int* p = (int*)malloc(sizeof(int));
    if (p) {
      *p = 1;
    }
  }
}

void no_directive_uninit_bad(void) {
  {
    int u;
    sink(u);
  }
}

void no_directive_dead_store_bad(void) {
  {
    int d = 42;
    d = 7;
    sink(d);
  }
}

void parallel_null_deref_bad(void) {
#pragma omp parallel
  {
    int* p = nullptr;
    *p = 1;
  }
}

void parallel_use_after_free_bad(void) {
#pragma omp parallel
  {
    int* p = (int*)malloc(sizeof(int));
    if (p) {
      free(p);
      *p = 1;
    }
  }
}

void parallel_leak_bad(void) {
#pragma omp parallel
  {
    int* p = (int*)malloc(sizeof(int));
    if (p) {
      *p = 1;
    }
  }
}

void parallel_uninit_bad(void) {
#pragma omp parallel
  {
    int u;
    sink(u);
  }
}

void parallel_dead_store_bad(void) {
#pragma omp parallel
  {
    int d = 42;
    d = 7;
    sink(d);
  }
}

void parallel_for_null_deref_bad(void) {
#pragma omp parallel for
  {
    for (int i = 0; i < 4; ++i) {
    int* p = nullptr;
      *p = 1;
    }
  }
}

void parallel_for_use_after_free_bad(void) {
#pragma omp parallel for
  {
    for (int i = 0; i < 4; ++i) {
    int* p = (int*)malloc(sizeof(int));
      if (p) {
        free(p);
        *p = 1;
      }
    }
  }
}

void FN_parallel_for_leak_bad(void) {
#pragma omp parallel for
  {
    for (int i = 0; i < 4; ++i) {
    int* p = (int*)malloc(sizeof(int));
      if (p) {
        *p = 1;
      }
    }
  }
}

void parallel_for_uninit_bad(void) {
#pragma omp parallel for
  {
    for (int i = 0; i < 4; ++i) {
    int u;
      sink(u);
    }
  }
}

void parallel_for_dead_store_bad(void) {
#pragma omp parallel for
  {
    for (int i = 0; i < 4; ++i) {
    int d = 42;
      d = 7;
      sink(d);
    }
  }
}

void for_in_parallel_null_deref_bad(void) {
#pragma omp parallel
  {
#pragma omp for
  {
    for (int i = 0; i < 4; ++i) {
    int* p = nullptr;
      *p = 1;
    }
  }
  }
}

void for_in_parallel_use_after_free_bad(void) {
#pragma omp parallel
  {
#pragma omp for
  {
    for (int i = 0; i < 4; ++i) {
    int* p = (int*)malloc(sizeof(int));
      if (p) {
        free(p);
        *p = 1;
      }
    }
  }
  }
}

void FN_for_in_parallel_leak_bad(void) {
#pragma omp parallel
  {
#pragma omp for
  {
    for (int i = 0; i < 4; ++i) {
    int* p = (int*)malloc(sizeof(int));
      if (p) {
        *p = 1;
      }
    }
  }
  }
}

void for_in_parallel_uninit_bad(void) {
#pragma omp parallel
  {
#pragma omp for
  {
    for (int i = 0; i < 4; ++i) {
    int u;
      sink(u);
    }
  }
  }
}

void for_in_parallel_dead_store_bad(void) {
#pragma omp parallel
  {
#pragma omp for
  {
    for (int i = 0; i < 4; ++i) {
    int d = 42;
      d = 7;
      sink(d);
    }
  }
  }
}

void task_null_deref_bad(void) {
#pragma omp task
  {
    int* p = nullptr;
    *p = 1;
  }
}

void task_use_after_free_bad(void) {
#pragma omp task
  {
    int* p = (int*)malloc(sizeof(int));
    if (p) {
      free(p);
      *p = 1;
    }
  }
}

void task_leak_bad(void) {
#pragma omp task
  {
    int* p = (int*)malloc(sizeof(int));
    if (p) {
      *p = 1;
    }
  }
}

void task_uninit_bad(void) {
#pragma omp task
  {
    int u;
    sink(u);
  }
}

void task_dead_store_bad(void) {
#pragma omp task
  {
    int d = 42;
    d = 7;
    sink(d);
  }
}

void single_null_deref_bad(void) {
#pragma omp single
  {
    int* p = nullptr;
    *p = 1;
  }
}

void single_use_after_free_bad(void) {
#pragma omp single
  {
    int* p = (int*)malloc(sizeof(int));
    if (p) {
      free(p);
      *p = 1;
    }
  }
}

void single_leak_bad(void) {
#pragma omp single
  {
    int* p = (int*)malloc(sizeof(int));
    if (p) {
      *p = 1;
    }
  }
}

void single_uninit_bad(void) {
#pragma omp single
  {
    int u;
    sink(u);
  }
}

void single_dead_store_bad(void) {
#pragma omp single
  {
    int d = 42;
    d = 7;
    sink(d);
  }
}

void critical_null_deref_bad(void) {
#pragma omp critical
  {
    int* p = nullptr;
    *p = 1;
  }
}

void critical_use_after_free_bad(void) {
#pragma omp critical
  {
    int* p = (int*)malloc(sizeof(int));
    if (p) {
      free(p);
      *p = 1;
    }
  }
}

void critical_leak_bad(void) {
#pragma omp critical
  {
    int* p = (int*)malloc(sizeof(int));
    if (p) {
      *p = 1;
    }
  }
}

void critical_uninit_bad(void) {
#pragma omp critical
  {
    int u;
    sink(u);
  }
}

void critical_dead_store_bad(void) {
#pragma omp critical
  {
    int d = 42;
    d = 7;
    sink(d);
  }
}

void master_null_deref_bad(void) {
#pragma omp master
  {
    int* p = nullptr;
    *p = 1;
  }
}

void master_use_after_free_bad(void) {
#pragma omp master
  {
    int* p = (int*)malloc(sizeof(int));
    if (p) {
      free(p);
      *p = 1;
    }
  }
}

void master_leak_bad(void) {
#pragma omp master
  {
    int* p = (int*)malloc(sizeof(int));
    if (p) {
      *p = 1;
    }
  }
}

void master_uninit_bad(void) {
#pragma omp master
  {
    int u;
    sink(u);
  }
}

void master_dead_store_bad(void) {
#pragma omp master
  {
    int d = 42;
    d = 7;
    sink(d);
  }
}

void simd_null_deref_bad(void) {
#pragma omp simd
  {
    for (int i = 0; i < 4; ++i) {
    int* p = nullptr;
      *p = 1;
    }
  }
}

void simd_use_after_free_bad(void) {
#pragma omp simd
  {
    for (int i = 0; i < 4; ++i) {
    int* p = (int*)malloc(sizeof(int));
      if (p) {
        free(p);
        *p = 1;
      }
    }
  }
}

void FN_simd_leak_bad(void) {
#pragma omp simd
  {
    for (int i = 0; i < 4; ++i) {
    int* p = (int*)malloc(sizeof(int));
      if (p) {
        *p = 1;
      }
    }
  }
}

void simd_uninit_bad(void) {
#pragma omp simd
  {
    for (int i = 0; i < 4; ++i) {
    int u;
      sink(u);
    }
  }
}

void simd_dead_store_bad(void) {
#pragma omp simd
  {
    for (int i = 0; i < 4; ++i) {
    int d = 42;
      d = 7;
      sink(d);
    }
  }
}

void sections_null_deref_bad(void) {
#pragma omp sections
  {
#pragma omp section
  {
    int* p = nullptr;
    *p = 1;
  }
  }
}

void sections_use_after_free_bad(void) {
#pragma omp sections
  {
#pragma omp section
  {
    int* p = (int*)malloc(sizeof(int));
    if (p) {
      free(p);
      *p = 1;
    }
  }
  }
}

void sections_leak_bad(void) {
#pragma omp sections
  {
#pragma omp section
  {
    int* p = (int*)malloc(sizeof(int));
    if (p) {
      *p = 1;
    }
  }
  }
}

void sections_uninit_bad(void) {
#pragma omp sections
  {
#pragma omp section
  {
    int u;
    sink(u);
  }
  }
}

void sections_dead_store_bad(void) {
#pragma omp sections
  {
#pragma omp section
  {
    int d = 42;
    d = 7;
    sink(d);
  }
  }
}

void taskgroup_null_deref_bad(void) {
#pragma omp taskgroup
  {
    int* p = nullptr;
    *p = 1;
  }
}

void taskgroup_use_after_free_bad(void) {
#pragma omp taskgroup
  {
    int* p = (int*)malloc(sizeof(int));
    if (p) {
      free(p);
      *p = 1;
    }
  }
}

void taskgroup_leak_bad(void) {
#pragma omp taskgroup
  {
    int* p = (int*)malloc(sizeof(int));
    if (p) {
      *p = 1;
    }
  }
}

void taskgroup_uninit_bad(void) {
#pragma omp taskgroup
  {
    int u;
    sink(u);
  }
}

void taskgroup_dead_store_bad(void) {
#pragma omp taskgroup
  {
    int d = 42;
    d = 7;
    sink(d);
  }
}

void ordered_region_null_deref_bad(void) {
#pragma omp parallel
  {
#pragma omp critical
  {
    int* p = nullptr;
    *p = 1;
  }
  }
}

void ordered_region_use_after_free_bad(void) {
#pragma omp parallel
  {
#pragma omp critical
  {
    int* p = (int*)malloc(sizeof(int));
    if (p) {
      free(p);
      *p = 1;
    }
  }
  }
}

void ordered_region_leak_bad(void) {
#pragma omp parallel
  {
#pragma omp critical
  {
    int* p = (int*)malloc(sizeof(int));
    if (p) {
      *p = 1;
    }
  }
  }
}

void ordered_region_uninit_bad(void) {
#pragma omp parallel
  {
#pragma omp critical
  {
    int u;
    sink(u);
  }
  }
}

void ordered_region_dead_store_bad(void) {
#pragma omp parallel
  {
#pragma omp critical
  {
    int d = 42;
    d = 7;
    sink(d);
  }
  }
}

// The region is before the bug: this reported correctly even when region bodies were dropped,
// and is here to show the loss was confined to the region rather than poisoning the function.
void after_region_bad(void) {
#pragma omp parallel
  { sink(0); }
  int* p = nullptr;
  *p = 1;
}

// The free is inside the region and the use is outside. With the body dropped the free was
// invisible, so this was reported as a leak instead of as the use-after-free it is.
void freed_in_region_used_after_bad(void) {
  int* p = (int*)malloc(sizeof(int));
  if (p) {
#pragma omp parallel
    { free(p); }
    *p = 1;
  }
}

// The region is what makes the pointer non-null. With the body dropped the pointer still held
// the null it was initialised with, so the null was blamed on the wrong line.
void assigned_in_region_used_after_bad(void) {
  int* p = nullptr;
#pragma omp parallel
  { p = (int*)malloc(sizeof(int)); }
  *p = 1;
}

// The region is what initialises the local: reporting it uninitialised afterwards is wrong.
void initialised_in_region_ok(void) {
  int u;
#pragma omp parallel
  { u = 7; }
  sink(u);
}

// The region is what frees it: reporting a leak, or a dead store to the pointer, is wrong.
void freed_in_region_ok(void) {
  int* p = (int*)malloc(sizeof(int));
#pragma omp parallel
  { free(p); }
}
