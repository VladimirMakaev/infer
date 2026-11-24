/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <stdint.h>
int nondet_a(void);
/* Does inferbo model a NARROWING cast at all? Each subject pairs the TRUE (truncated)
   value with the UNTRUNCATED one, so the two answers tell different stories:
     silent on true + report on untruncated = the cast is ignored, the wide value kept
     silent on both                         = the value is lost entirely
   Narrowing to a SIGNED type when the value does not fit is implementation-defined in
   C17; these signed oracles use the LLVM/Clang two-complement target contract.
   The uint8 case is fully DEFINED by the standard, so it carries no caveat at all. */

/* S1 (int8_t)300 == 44 at run time. TRUE value. */
int conversion_s1_i8_true_bad(void)  { int64_t x = nondet_a()?300:300; int8_t v=(int8_t)x;
                        int a[1] = {0}; if (v == 44) { a[1]=1; } return a[0]; }
/* S2 the untruncated value, FALSE at run time. */
int conversion_s2_i8_untrunc_ok(void){ int64_t x = nondet_a()?300:300; int8_t v=(int8_t)x;
                        int a[1] = {0}; if (v == 300) { a[1]=1; } return a[0]; }
/* S3 reach control in the SAME shape: unconditional. Must report. */
int conversion_s3_i8_reach_bad(void) { int64_t x = nondet_a()?300:300; int8_t v=(int8_t)x;
                        int a[1] = {0}; a[1]=1; return a[0]+(int)v; }

/* S4 UNSIGNED narrowing, fully defined by the standard: (uint8_t)300 == 44. */
int conversion_s4_u8_true_bad(void)  { int64_t x = nondet_a()?300:300; uint8_t v=(uint8_t)x;
                        int a[1] = {0}; if (v == 44) { a[1]=1; } return a[0]; }
int conversion_s5_u8_untrunc_ok(void){ int64_t x = nondet_a()?300:300; uint8_t v=(uint8_t)x;
                        int a[1] = {0}; if (v == 300) { a[1]=1; } return a[0]; }
int conversion_s6_u8_reach_bad(void) { int64_t x = nondet_a()?300:300; uint8_t v=(uint8_t)x;
                        int a[1] = {0}; a[1]=1; return a[0]+(int)v; }

/* S7 int64 -> int32, a very common narrowing: (int32_t)4294967296 == 0. */
int conversion_s7_i32_true_bad(void) { int64_t x = nondet_a()?4294967296LL:4294967296LL; int32_t v=(int32_t)x;
                        int a[1] = {0}; if (v == 0) { a[1]=1; } return a[0]; }
int conversion_s8_i32_untrunc_ok(void){ int64_t x = nondet_a()?4294967296LL:4294967296LL; int32_t v=(int32_t)x;
                        int a[1] = {0}; if (v == 4294967296LL) { a[1]=1; } return a[0]; }
int conversion_s9_i32_reach_bad(void){ int64_t x = nondet_a()?4294967296LL:4294967296LL; int32_t v=(int32_t)x;
                        int a[1] = {0}; a[1]=1; return a[0]+(int)v; }

/* S10 CONTROL: a cast that loses nothing. (int8_t)42 == 42. Must report -- this shows
   the cast shape itself is handled and only the TRUNCATING case is the problem. */
int conversion_s10_i8_fits_bad(void) { int64_t x = nondet_a()?42:42; int8_t v=(int8_t)x;
                        int a[1] = {0}; if (v == 42) { a[1]=1; } return a[0]; }

/* S11 THE CONSEQUENCE: use the narrowed value as an index. At run time
   (int8_t)300 is 44, so arr[44] on int arr[50] is IN BOUNDS and this is safe.
   If inferbo keeps 300 it should report a false overrun. */
int conversion_s11_index_fp_ok(void) { int64_t x = nondet_a()?300:300; int8_t v=(int8_t)x;
                         int arr[50] = {0}; arr[v] = 1; return arr[0]; }
/* S12 the mirror: (int8_t)200 is -56, so arr[v+56] is arr[0], in bounds; but if the
   cast is ignored the index looks like 256 and a false overrun should appear. */
int conversion_s12_index_fp2_ok(void){ int64_t x = nondet_a()?200:200; int8_t v=(int8_t)x;
                         int arr[50] = {0}; arr[v+56] = 1; return arr[0]; }
