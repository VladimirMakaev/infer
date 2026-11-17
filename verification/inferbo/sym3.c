#include <stdint.h>
#include <stdlib.h>

/* idx=0: OOB at n in [] (none) */
int k0(int64_t n) {
  if (n < 1 || n > 8) return 0;
  char *b = (char *)malloc(n); if (!b) return 0;
  b[0] = 1; int r = b[0]; free(b); return r;
}

/* idx=1: OOB at n in [1] (some) */
int k1(int64_t n) {
  if (n < 1 || n > 8) return 0;
  char *b = (char *)malloc(n); if (!b) return 0;
  b[1] = 1; int r = b[0]; free(b); return r;
}

/* idx=2: OOB at n in [1, 2] (some) */
int k2(int64_t n) {
  if (n < 1 || n > 8) return 0;
  char *b = (char *)malloc(n); if (!b) return 0;
  b[2] = 1; int r = b[0]; free(b); return r;
}

/* idx=3: OOB at n in [1, 2, 3] (some) */
int k3(int64_t n) {
  if (n < 1 || n > 8) return 0;
  char *b = (char *)malloc(n); if (!b) return 0;
  b[3] = 1; int r = b[0]; free(b); return r;
}

/* idx=4: OOB at n in [1, 2, 3, 4] (some) */
int k4(int64_t n) {
  if (n < 1 || n > 8) return 0;
  char *b = (char *)malloc(n); if (!b) return 0;
  b[4] = 1; int r = b[0]; free(b); return r;
}

/* idx=5: OOB at n in [1, 2, 3, 4, 5] (some) */
int k5(int64_t n) {
  if (n < 1 || n > 8) return 0;
  char *b = (char *)malloc(n); if (!b) return 0;
  b[5] = 1; int r = b[0]; free(b); return r;
}

/* idx=6: OOB at n in [1, 2, 3, 4, 5, 6] (some) */
int k6(int64_t n) {
  if (n < 1 || n > 8) return 0;
  char *b = (char *)malloc(n); if (!b) return 0;
  b[6] = 1; int r = b[0]; free(b); return r;
}

/* idx=7: OOB at n in [1, 2, 3, 4, 5, 6, 7] (some) */
int k7(int64_t n) {
  if (n < 1 || n > 8) return 0;
  char *b = (char *)malloc(n); if (!b) return 0;
  b[7] = 1; int r = b[0]; free(b); return r;
}

/* idx=8: OOB at n in [1, 2, 3, 4, 5, 6, 7, 8] (ALL) */
int k8(int64_t n) {
  if (n < 1 || n > 8) return 0;
  char *b = (char *)malloc(n); if (!b) return 0;
  b[8] = 1; int r = b[0]; free(b); return r;
}

/* idx=9: OOB at n in [1, 2, 3, 4, 5, 6, 7, 8] (ALL) */
int k9(int64_t n) {
  if (n < 1 || n > 8) return 0;
  char *b = (char *)malloc(n); if (!b) return 0;
  b[9] = 1; int r = b[0]; free(b); return r;
}
