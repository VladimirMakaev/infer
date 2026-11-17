#include <stdint.h>
#include <stdlib.h>
/* Ladder isolating the missed overrun at n=2: size = n-1, idx = n/2.
   At n=2 the buffer is 1 byte and the index is 1, so buf[1] is out of bounds.
   Each rung removes one symbolic ingredient to find which one loses the report. */

/* A: as generated. n in [2,8]. SILENT in the grid. */
int a_both_symbolic(int64_t n) {
  if (n < 2 || n > 8) return 0;
  char *b = (char *)malloc(n - 1); if (!b) return 0;
  b[n / 2] = 1; int r = b[0]; free(b); return r;
}
/* B: n pinned to exactly 2. Everything is concrete; must report. */
int b_n_is_exactly_2(int64_t n) {
  if (n < 2 || n > 2) return 0;
  char *b = (char *)malloc(n - 1); if (!b) return 0;
  b[n / 2] = 1; int r = b[0]; free(b); return r;
}
/* C: the range narrowed to [2,3]; still contains the bad n. */
int c_range_2_3(int64_t n) {
  if (n < 2 || n > 3) return 0;
  char *b = (char *)malloc(n - 1); if (!b) return 0;
  b[n / 2] = 1; int r = b[0]; free(b); return r;
}
/* D: symbolic size, CONSTANT index 1. At n=2 the size is 1, so b[1] is out of bounds. */
int d_const_index(int64_t n) {
  if (n < 2 || n > 8) return 0;
  char *b = (char *)malloc(n - 1); if (!b) return 0;
  b[1] = 1; int r = b[0]; free(b); return r;
}
/* E: CONSTANT size 1, symbolic index n/2 (>= 1 throughout the range). */
int e_const_size(int64_t n) {
  if (n < 2 || n > 8) return 0;
  char *b = (char *)malloc(1); if (!b) return 0;
  b[n / 2] = 1; int r = b[0]; free(b); return r;
}
/* F: same shape as A but with the division replaced by a subtraction, so the only
   difference from A is the operation producing the index. At n=2, n-1 = 1 = size. */
int f_minus_instead_of_div(int64_t n) {
  if (n < 2 || n > 8) return 0;
  char *b = (char *)malloc(n - 1); if (!b) return 0;
  b[n - 1] = 1; int r = b[0]; free(b); return r;
}
/* CONTROL: unconditionally out of bounds, proves the shape and malloc are modelled. */
int ctl_unconditional(int64_t n) {
  if (n < 2 || n > 8) return 0;
  char *b = (char *)malloc(n - 1); if (!b) return 0;
  b[-1] = 1; int r = b[0]; free(b); return r;
}
