#include <stdint.h>
#include <stdlib.h>
/* Does inferbo report a POSSIBLE (not certain) overrun? Separate the two candidate
   explanations for sym3: "only certain overruns are reported" vs "a symbolic SIZE
   loses the constraint". Every case below is a genuine possible-but-not-certain
   overrun except the controls. */

/* P1 concrete size, symbolic index: OOB only at i == 8. */
int p1_concrete_size_symbolic_index(int64_t i) {
  char b[8];
  if (i < 0 || i > 8) return 0;
  b[i] = 1; return b[0];
}
/* P2 symbolic size, constant index: OOB only at n == 1. (sym3's k=1) */
int p2_symbolic_size_const_index(int64_t n) {
  if (n < 1 || n > 8) return 0;
  char *b = (char *)malloc(n); if (!b) return 0;
  b[1] = 1; int r = b[0]; free(b); return r;
}
/* P3 symbolic size, index that is a SYMBOL pinned to the same value as P2's literal. */
int p3_symbolic_size_pinned_symbolic_index(int64_t n, int64_t j) {
  if (n < 1 || n > 8) return 0;
  if (j < 1 || j > 1) return 0;
  char *b = (char *)malloc(n); if (!b) return 0;
  b[j] = 1; int r = b[0]; free(b); return r;
}
/* P4 symbolic size, symbolic index, both ranging: OOB whenever j >= n. */
int p4_symbolic_both(int64_t n, int64_t j) {
  if (n < 1 || n > 8) return 0;
  if (j < 0 || j > 8) return 0;
  char *b = (char *)malloc(n); if (!b) return 0;
  b[j] = 1; int r = b[0]; free(b); return r;
}
/* C1 control: concrete size, index certainly out of bounds. */
int c1_concrete_certain(int64_t i) {
  char b[8];
  if (i < 8 || i > 9) return 0;
  b[i] = 1; return b[0];
}
/* C2 control: symbolic size, index certainly out of bounds (>= every possible size). */
int c2_symbolic_certain(int64_t n) {
  if (n < 1 || n > 8) return 0;
  char *b = (char *)malloc(n); if (!b) return 0;
  b[8] = 1; int r = b[0]; free(b); return r;
}
