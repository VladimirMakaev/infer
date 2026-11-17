#include <stdint.h>

/* FINDING 3: ItvPure.band_sem's both-non-positive branch has an unsound UPPER bound.
 *
 *   else if is_le_zero x && is_le_zero y then (Bound.minf, overapprox_min (ub x) (ub y))
 *
 * `x & y <= min(x,y)` is true for two STRICTLY NEGATIVE values (ANDing only clears
 * bits, and clearing a bit in a two's-complement negative lowers it). The guard is
 * `<= 0`, so it also admits 0 -- and `0 & y` is 0, which is ABOVE min(ub x, ub y)
 * whenever that is negative. Nothing in C is undefined here: & is total.
 *
 * Every function below is free of undefined behaviour. */

/* ---- S1: the excluded value, stated directly. -------------------------------
 * `x & y` is identically 0. a[1] on int a[1] is an unconditional overrun, so this
 * reports iff inferbo thinks v can be 0. SILENT = inferbo excluded the only value
 * the expression can take. */
int s1_band_excludes_zero(int d) {
  int64_t x = 0;
  int64_t y = d ? -3 : -2;
  int64_t v = x & y;
  if (v == 0) { int a[1]; a[1] = 1; return a[0]; }
  return 0;
}

/* ---- S2: the same defect WITHOUT a constant operand. ------------------------
 * x spans [-1,0] so get_const x is None and is_ge_zero x is false: branch 3
 * ("is_ge_zero x -> (zero, ub x)"), which would have been both sound and precise,
 * cannot rescue it. Achievable results are {-3,-2,0}; 0 is again excluded. */
int s2_band_no_const_operand(int c, int d) {
  int64_t x = c ? -1 : 0;
  int64_t y = d ? -3 : -2;
  int64_t v = x & y;
  if (v == 0) { int a[1]; a[1] = 1; return a[0]; }
  return 0;
}

/* ---- S3: THE CONSEQUENCE -- a real buffer overrun that inferbo does not report.
 * i is identically 0. The `i >= -8` test is TRUE at run time, so the body really
 * does execute and really does write a[8] on an array of 7 ints.
 * inferbo has i in [-inf,-2]; pruning i >= -8 gives [-8,-2], so it believes
 * i + 8 is in [0,6] -- entirely inside a[7] -- and proves the write safe.
 * SILENCE HERE IS A MISSED BUFFER OVERRUN, not merely a lost value. */
int s3_missed_overrun(int d) {
  int64_t x = 0;
  int64_t y = d ? -3 : -2;
  int64_t i = x & y;
  if (i >= -8) {
    int a[7];
    a[i + 8] = 1;
    return a[0];
  }
  return 0;
}

/* ---- C1: firing control for S3's access shape. ------------------------------
 * Identical to S3 except i comes from a literal instead of the AND. The write is
 * the same a[8] on a[7]. This MUST report; if it does not, S3's silence proves
 * nothing about band_sem and only shows the shape is invisible. */
int c1_same_access_without_band(void) {
  int64_t i = 0;
  if (i >= -8) {
    int a[7];
    a[i + 8] = 1;
    return a[0];
  }
  return 0;
}

/* ---- C2: firing control for the `v == 0` discriminator. ---------------------
 * Same guard, same body, but v comes from an operation inferbo gets right.
 * MUST report. */
int c2_guard_fires_when_zero_is_kept(int d) {
  int64_t x = 0;
  int64_t y = d ? 3 : 2;
  int64_t v = x & y; /* both operands non-negative: branch 1, [0, min(ub)] -> keeps 0 */
  if (v == 0) { int a[1]; a[1] = 1; return a[0]; }
  return 0;
}

/* ---- C3: the branch is sound where its lemma actually holds. ----------------
 * Both operands STRICTLY negative. -3 & -2 = -4, min(-3,-2) = -3, and -4 <= -3, so
 * the upper bound is correct here. This must REPORT (i.e. -4 is kept), which is the
 * evidence that the defect is the zero boundary specifically and not the branch as
 * a whole -- a control that distinguishes the two explanations. */
int c3_strictly_negative_is_sound(int c, int d) {
  int64_t x = c ? -3 : -3;
  int64_t y = d ? -2 : -2;
  int64_t v = x & y; /* identically -4 */
  if (v == -4) { int a[1]; a[1] = 1; return a[0]; }
  return 0;
}
