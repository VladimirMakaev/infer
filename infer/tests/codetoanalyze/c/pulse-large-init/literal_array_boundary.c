/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#define R2(x) x, x
#define R4(x) R2(x), R2(x)
#define R16(x) R4(x), R4(x), R4(x), R4(x)
#define R32(x) R16(x), R16(x)
#define R64(x) R32(x), R32(x)
#define R128(x) R64(x), R64(x)
#define R256(x) R128(x), R128(x)
#define R512(x) R256(x), R256(x)
#define R1024(x) R512(x), R512(x)
#define R2048(x) R1024(x), R1024(x)
#define R4096(x) R2048(x), R2048(x)
#define R8192(x) R4096(x), R4096(x)
#define R10000(x) R8192(x), R1024(x), R512(x), R256(x), R16(x)

void use(int);

// At the limit the exact values must survive export and translation.
void deref_at_array_limit_bad() {
  int* a[10000] = {R10000(0)};
  *a[9999] = 1;
}

// An omitted list with a filler is still an initialization, not an empty list.
void read_large_array_filler_ok() {
  int a[20000] = {R10000(1), 1};
  use(a[19999]);
}
