#include <stdint.h>

/* Reachability: the defect does not need a literal 0 operand. This is the standard
 * branchless-select / constant-time idiom, where a mask is built as 0 or -1 by
 * negating a predicate. Both operands are then non-positive and the mask can be 0,
 * which is exactly band_sem's broken branch. */

/* mask is 0 or -1; v is 0 when cond is false. A real a[8] write into int a[7]. */
int s4_mask_idiom_missed_overrun(int cond, int d) {
  int64_t mask = -(int64_t)(cond != 0); /* 0 or -1, no literal 0 operand */
  int64_t val = d ? -3 : -2;
  int64_t i = mask & val; /* achievable: 0 (cond false), -3, -2 */
  if (i >= -8) {          /* true at run time on every path */
    int a[7];
    a[i + 8] = 1; /* cond false -> i = 0 -> a[8] -> REAL OVERRUN */
    return a[0];
  }
  return 0;
}

/* C4: firing control -- same function with the AND replaced by the achievable value
 * that the mask path produces. Must report. */
int c4_mask_idiom_control(void) {
  int64_t i = 0;
  if (i >= -8) {
    int a[7];
    a[i + 8] = 1;
    return a[0];
  }
  return 0;
}
