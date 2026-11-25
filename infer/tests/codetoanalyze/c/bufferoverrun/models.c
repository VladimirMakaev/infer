/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
#include <stdarg.h>
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <unistd.h>

void exit_bo_good_unreachable_bad() {
  int arr[1];
  exit(1);
  // unreachable so no buffer overrun
  arr[42] = 42;
}

void fgetc_m1_bad(FILE* f) {
  int arr[10000];
  int c = fgetc(f);
  arr[c] = 42;
}

void fgetc_255_bad(FILE* f) {
  int arr[255];
  int c = fgetc(f);
  if (c >= 0) {
    arr[c] = 42;
  }
}

void fgetc_256_good(FILE* f) {
  int arr[256];
  int c = fgetc(f);
  if (c >= 0) {
    arr[c] = 42;
  }
}

void fgetc_256_bad(FILE* f) {
  int arr[256];
  int c = fgetc(f);
  arr[c + 1] = 42;
}

void fgetc_257_good(FILE* f) {
  int arr[257];
  int c = fgetc(f);
  arr[c + 1] = 42;
}

void memcpy_bad1() {
  int arr1[10];
  int arr2[20];
  memcpy(arr1, arr2, 44);
}

void memcpy_bad2() {
  int arr1[10];
  int arr2[20];
  memcpy(arr2, arr1, 44);
}

void memcpy_bad3() {
  int arr1[10];
  int arr2[20];
  memcpy(arr1, arr2, -1);
}

void memcpy_bad4() {
  int src[1];
  int buff[1];
  int* dst = &buff[0];
  memcpy(dst, src, sizeof(dst));
}

void memcpy_good1() {
  int arr1[10];
  int arr2[20];
  memcpy(arr2, arr1, 40);
}

void memcpy_good2() {
  int arr1[10];
  int arr2[20];
  memcpy(arr2, arr1, 0);
}

void memcpy_good3() {
  int arr1[10];
  int arr2[20];
  memcpy(arr2, arr1, 20);
}

void memcpy_good4() {
  int src[3];
  int dst[3];
  memcpy(dst, src, sizeof(dst));
}

void memcpy_len_Good(size_t len) {
  char dst[len];
  char src[len];
  memcpy(dst, src, len);
}

void call_memcpy_len1_Good() { memcpy_len_Good(40); }

extern size_t unknown_uint();

void call_memcpy_len2_Good() {
  size_t x = unknown();
  memcpy_len_Good(x);
}

void FN_symbolic_signed_size_bad() {
  // A negative signed result becomes a huge size_t. Equal copy lengths don't
  // make these VLAs safe to allocate.
  // Ignoring the symbolic conversion can hide the large allocation risk.
  size_t x = unknown();
  memcpy_len_Good(x);
}

void memmove_bad1() {
  int arr1[10];
  int arr2[20];
  memmove(arr1, arr2, 44);
}

void memmove_bad2() {
  int arr1[10];
  int arr2[20];
  memmove(arr2, arr1, 44);
}

void memmove_bad3() {
  int arr1[10];
  int arr2[20];
  memmove(arr1, arr2, -1);
}

void memmove_bad4() {
  int src[1];
  int buff[1];
  int* dst = &buff[0];
  memmove(dst, src, sizeof(dst));
}

void memmove_good1() {
  int arr1[10];
  int arr2[20];
  memmove(arr2, arr1, 40);
}

void memmove_good2() {
  int arr1[10];
  int arr2[20];
  memmove(arr2, arr1, 0);
}

void memmove_good3() {
  int arr1[10];
  int arr2[20];
  memmove(arr2, arr1, 20);
}

void memmove_good4() {
  int src[3];
  int dst[3];
  memmove(dst, src, sizeof(dst));
}

void memset_bad1() {
  int arr[10];
  memset(arr, 0, 44);
}

void memset_bad2() {
  int arr[10];
  memset(arr, 0, -1);
}

void memset_bad3() {
  int arr[1];
  int* dst = &arr[0];
  memset(dst, 0, sizeof(dst));
}

void memset_good1() {
  int arr[10];
  memset(arr, 0, 40);
}

void memset_good2() {
  int arr[10];
  memset(arr, 0, 0);
}

void memset_good3() {
  int arr[10];
  memset(arr, 0, 20);
}

void memset_good4() {
  int arr[10];
  memset(arr, 0, sizeof(arr));
}

void strncpy_bad1() {
  int arr1[10];
  int arr2[20];
  strncpy(arr1, arr2, 44);
}

// No string operation wrote the source, which is then assumed terminated.
void strncpy_bad2_FN() {
  int arr1[10];
  int arr2[20];
  strncpy(arr2, arr1, 44);
}

void strncpy_bad3() {
  int arr1[10];
  int arr2[20];
  strncpy(arr1, arr2, -1);
}

void strncpy_bad4() {
  int src[1];
  int buff[1];
  int* dst = &buff[0];
  strncpy(dst, src, sizeof(dst));
}

void strncpy_good1() {
  int arr1[10];
  int arr2[20];
  strncpy(arr2, arr1, 40);
}

void strncpy_good2() {
  int arr1[10];
  int arr2[20];
  strncpy(arr2, arr1, 0);
}

void strncpy_good3() {
  int arr1[10];
  int arr2[20];
  strncpy(arr2, arr1, 20);
}

void strncpy_good4() {
  int src[3];
  int dst[3];
  strncpy(dst, src, sizeof(dst));
}

void strncpy_good5_FP() {
  char src[5] = "test";
  char dst[5];
  strncpy(dst, src, 10);
}

void strncpy_short_literal_Good() {
  char dst[16];
  const char* src = "abc";
  strncpy(dst, src, 15);
}

void strncpy_short_array_Good() {
  char src[8] = "abc";
  char dst[16];
  strncpy(dst, src, 15);
}

const char short_global[] = "abc";

void strncpy_short_global_Good() {
  char dst[16];
  strncpy(dst, short_global, 15);
}

void strncpy_short_src_dst_too_small_Bad() {
  char dst[8];
  strncpy(dst, "abc", 15);
}

const char unterminated_global[4] = {'a', 'b', 'c', 'd'};

void strncpy_unterminated_global_Bad() {
  char dst[16];
  strncpy(dst, unterminated_global, 15);
}

const char literal_unterminated_global[3] = "abc";

void strncpy_literal_unterminated_global_Bad() {
  char dst[16];
  strncpy(dst, literal_unterminated_global, 15);
}

void strncpy_unterminated_Bad() {
  char src[40];
  memset(src, 'a', sizeof(src));
  char dst[100];
  strncpy(dst, src, 44);
}

void strncpy_read_Bad(int fd) {
  char src[40];
  read(fd, src, sizeof(src));
  char dst[100];
  strncpy(dst, src, 44);
}

void strncpy_zeroed_read_Bad(int fd) {
  char src[40] = {0};
  read(fd, src, sizeof(src));
  char dst[100];
  strncpy(dst, src, 44);
}

void strncpy_literal_memset_Bad() {
  char src[40] = "abc";
  memset(src, 'a', sizeof(src));
  char dst[100];
  strncpy(dst, src, 44);
}

void strncpy_literal_memcpy_Bad() {
  char src[40] = "abc";
  char raw[40];
  memset(raw, 'a', sizeof(raw));
  memcpy(src, raw, sizeof(src));
  char dst[100];
  strncpy(dst, src, 44);
}

void strncpy_readv_Bad(int fd) {
  char src[40] = {0};
  struct iovec iov = {src, sizeof(src)};
  readv(fd, &iov, 1);
  char dst[100];
  strncpy(dst, src, 44);
}

size_t unknown_convert(char** in, char** out);

void strncpy_out_ptr_Bad(char* in) {
  char src[40] = "";
  char* out = src;
  unknown_convert(&in, &out);
  char dst[100];
  strncpy(dst, src, 44);
}

struct chars_holder {
  char* p;
};

void unknown_fill_holder(struct chars_holder* h);

void strncpy_holder_Bad() {
  char src[40] = "";
  struct chars_holder h = {src};
  unknown_fill_holder(&h);
  char dst[100];
  strncpy(dst, src, 44);
}

void unknown_fill_ints(int* p);

void strncpy_int_ptr_Bad() {
  char src[40] = "";
  unknown_fill_ints((int*)src);
  char dst[100];
  strncpy(dst, src, 44);
}

// Only stores of characters update the length of the string in an array.
void FN_strncpy_int_stores_Bad() {
  char src[40] = {0};
  int* p = (int*)src;
  for (int i = 0; i < 10; i++) {
    p[i] = 0x61616161;
  }
  char dst[100];
  strncpy(dst, src, 44);
}

void strncpy_literal_Good() {
  char src[40] = "abc";
  char dst[100];
  strncpy(dst, src, 44);
}

void strncpy_literal_puts_Good() {
  char src[40] = "abc";
  puts(src);
  char dst[100];
  strncpy(dst, src, 44);
}

void strncpy_strcpy_Good() {
  char src[40];
  strcpy(src, "abc");
  char dst[100];
  strncpy(dst, src, 44);
}

void memcpy_contents_Good() {
  int src[3] = {5, 5, 5};
  int dst[3];
  memcpy(dst, src, sizeof(dst));
  int a[6];
  a[dst[0]] = 0;
}

void memcpy_contents_Bad() {
  int src[3] = {5, 5, 5};
  int dst[3];
  memcpy(dst, src, sizeof(dst));
  int a[5];
  a[dst[0]] = 0;
}

void memcpy_integer_Good() {
  int src = 5;
  int dst;
  memcpy(&dst, &src, sizeof(int));
  int a[10];
  a[dst] = 0;
}

void memcpy_integer_Bad() {
  int src = 5;
  int dst;
  memcpy(&dst, &src, sizeof(int));
  int a[5];
  a[dst] = 0;
}

void strncpy_contents_Good() {
  char src[5] = "test";
  char dst[10];
  strncpy(dst, src, 5);
  int a[5];
  a[strlen(dst)] = 0;
}

void strncpy_contents_Bad() {
  char src[5] = "test";
  char dst[10];
  strncpy(dst, src, 5);
  int a[4];
  a[strlen(dst)] = 0;
}

void strncpy_no_null_1_Bad_FN() {
  char src[5] = "test";
  src[4] = 'a';
  char dst[10];
  strncpy(dst, src, 5); // [dst] may not have null character.
  int a[10];
  a[strlen(dst)] = 0;
}

void strncpy_no_null_2_Bad() {
  char src[5] = "test";
  src[4] = 'a';
  char dst[10];
  strncpy(dst, src, 5); // [dst] may not have null character.
  int a[5];
  a[strlen(dst)] = 0;
}

void strncpy_no_null_3_Bad_FN() {
  char src[15] = "test.test.test";
  char dst[10];
  strncpy(dst, src, 10); // [dst] does not have null character.
  int a[20];
  a[strlen(dst)] = 0;
}

void strncpy_no_null_4_Bad() {
  char src[15] = "test.test.test";
  char dst[10];
  strncpy(dst, src, 10); // [dst] does not have null character.
  int a[10];
  a[strlen(dst)] = 0;
}

void strndup_1_Good() {
  char s1[] = "Hello Infer!";
  int len = strlen(s1);
  char* s2 = strndup(s1, len);
  for (int i = 0; i < len; i++) {
    s2[i] = 'a';
  }
}

void strndup_1_Bad() {
  char s1[] = "Hello Infer!";
  int len = 100;
  char* s2 = strndup(s1, len);
  for (int i = 0; i < len; i++) {
    s2[i] = 'a';
  }
}

void strndup_2_Good() {
  char s1[] = "Hello Infer!";
  int len = strlen(s1);
  s1[5] = '\0';
  char* s2 = strndup(s1, len);
  for (int i = 0; i < strlen(s2); i++) {
    s2[i] = 'a';
  }
}

void strndup_2_Bad() {
  char s1[] = "Hello Infer!";
  int len = strlen(s1);
  s1[5] = '\0';
  char* s2 = strndup(s1, len);
  for (int i = 0; i < len; i++) {
    s2[i] = 'a';
  }
}

void strndup_3_Good() {
  int size = unknown();
  if (size >= 10) {
    char* s1 = (char*)malloc(sizeof(char) * size);
    s1[5] = '\0';
    char* s2 = strndup(s1, size - 1);
    for (int i = 0; i < strlen(s2); i++) {
      s2[i] = 'a';
    }
  }
}

void strndup_3_Bad() {
  int size = unknown();
  if (size >= 10) {
    char* s1 = (char*)malloc(sizeof(char) * size);
    s1[5] = '\0';
    char* s2 = strndup(s1, size - 1);
    for (int i = 0; i < size - 1; i++) {
      s2[i] = 'a';
    }
  }
}

void strcpy_Good() {
  char src[5] = "test";
  char dst[10];
  strcpy(dst, src);
}

void strcpy_Bad() {
  char src[5] = "test";
  char dst[4];
  strcpy(dst, src);
}

void strcpy_literal_string_Good() {
  char dst[10];
  strcpy(dst, "test");
}

void strcpy_literal_string_Bad() {
  char dst[4];
  strcpy(dst, "test");
}

void strcpy_strlen_Good() {
  char src[5] = "test";
  char dst[10];
  strcpy(dst, src);
  int a[5];
  a[strlen(dst)] = 0;
}

void strcpy_strlen_Bad() {
  char src[5] = "test";
  char dst[10];
  strcpy(dst, src);
  int a[4];
  a[strlen(dst)] = 0;
}

void strcpy_contents_Good() {
  char src[5] = "aaaa";
  char dst[5];
  strcpy(dst, src);
  char c = dst[0];
  if (c >= 'a') {
    int a[5];
    a[c - 'a' + 4] = 0;
  }
}

void strcpy_contents_Bad() {
  char src[5] = "aaaa";
  char dst[5];
  strcpy(dst, src);
  char c = dst[0];
  if (c >= 'a') {
    int a[5];
    a[c - 'a' + 5] = 0;
  }
}

void strcpy_no_null_Bad() {
  char src[5] = "test";
  src[4] = 'a';
  char dst[10];
  strcpy(dst, src);
}

void strcat_Good() {
  char str[8];
  strcpy(str, "abc");
  strcat(str, "defg");
}

void strcat_Bad() {
  char str[8];
  strcpy(str, "abc");
  strcat(str, "defgh");
}

void strcat_strlen_Good() {
  char str[8];
  strcpy(str, "abc");
  strcat(str, "defg");
  int a[8];
  a[strlen(str)] = 0;
}

void strcat_strlen_Bad() {
  char str[20];
  strcpy(str, "abc");
  strcat(str, "defgh");
  int a[8];
  a[strlen(str)] = 0;
}

void snprintf_sizeof_Good() {
  char dst[8];
  snprintf(dst, sizeof(dst), "%s", "abc");
}

void snprintf_exact_size_Good() {
  char dst[8];
  snprintf(dst, 8, "%s", "abc");
}

void snprintf_zero_size_Good() {
  char dst[8];
  snprintf(dst, 0, "%s", "abc");
}

void snprintf_size_off_by_one_Bad() {
  char dst[8];
  snprintf(dst, 9, "%s", "abc");
}

void snprintf_size_of_other_buffer_Bad() {
  char dst[8];
  char src[64];
  snprintf(dst, sizeof(src), "%s", src);
}

void vsnprintf_sizeof_Good(const char* fmt, va_list ap) {
  char dst[8];
  vsnprintf(dst, sizeof(dst), fmt, ap);
}

void vsnprintf_size_too_big_Bad(const char* fmt, va_list ap) {
  char dst[8];
  vsnprintf(dst, 64, fmt, ap);
}

void strncat_Good() {
  char str[16];
  strcpy(str, "abcd");
  strncat(str, "efgh", 4);
}

void strncat_exact_fit_Good() {
  char str[9];
  strcpy(str, "abcd");
  strncat(str, "efgh", 4);
}

void strncat_bounded_by_size_Good() {
  char str[8];
  strcpy(str, "abcd");
  strncat(str, "efghijkl", 3);
}

void strncat_bounded_by_src_Good() {
  char str[8];
  strcpy(str, "abcd");
  strncat(str, "ef", 99);
}

// strncat stops after size characters, so the source need not be
// null-terminated and its last readable character sits at index size-1
void strncat_source_not_terminated_Good() {
  char src[4] = {'a', 'b', 'c', 'd'};
  char str[16];
  strcpy(str, "x");
  strncat(str, src, 4);
}

void strncat_source_not_terminated_sizeof_Good() {
  char src[8] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};
  char str[32];
  strcpy(str, "x");
  strncat(str, src, sizeof(src));
}

void strncat_source_size_past_end_Bad() {
  char src[4] = {'a', 'b', 'c', 'd'};
  char str[64];
  strcpy(str, "x");
  strncat(str, src, 8);
}

void strncat_Bad() {
  char str[8];
  strcpy(str, "abcd");
  strncat(str, "efgh", 4);
}

void strncat_size_too_big_Bad() {
  char str[8];
  strcpy(str, "abcd");
  strncat(str, "efghijkl", 8);
}

void strncat_strlen_Good() {
  char str[16];
  strcpy(str, "abc");
  strncat(str, "defg", 4);
  int a[8];
  a[strlen(str)] = 0;
}

void strncat_strlen_Bad() {
  char str[20];
  strcpy(str, "abc");
  strncat(str, "defgh", 5);
  int a[8];
  a[strlen(str)] = 0;
}

void snprintf_constant_offset_Bad() {
  char dst[8];
  snprintf(dst + 2, 64, "%s", "x");
}

void snprintf_constant_offset_Good() {
  char dst[64];
  snprintf(dst + 2, 8, "%s", "x");
}


int snprintf_unknown_offset(void);

// The offset is not known, but the size does not fit the destination at any offset, so
// there is nothing to correlate and the overflow is still reported.
void snprintf_unknown_offset_oversize_Bad() {
  char dst[8];
  int i = snprintf_unknown_offset();
  if (i < 0 || i > 3) {
    return;
  }
  snprintf(dst + i, 100, "%s", "x");
}

// The size fits the destination, so only the offset can push the write past its end.
void snprintf_constant_offset_fitting_size_Bad() {
  char dst[8];
  snprintf(dst + 6, 8, "%s", "x");
}

// The offset is not known and the size fits the destination, but a size the caller spelled
// out did not come from the offset, so the two can be added up and the overflow reported.
void snprintf_unknown_offset_fitting_size_Bad() {
  char dst[8];
  int i = snprintf_unknown_offset();
  if (i < 0 || i > 3) {
    return;
  }
  snprintf(dst + i, 8, "%s", "x");
}

// The append idiom: the size is what is left of the destination, so the write stops at its
// end however far in the unknown offset is.
void snprintf_derived_size_Good() {
  char dst[8];
  int i = snprintf_unknown_offset();
  if (i < 0 || i > 3) {
    return;
  }
  snprintf(dst + i, 8 - i, "%s", "x");
}

void sprintf_literal_fits_Good() {
  char dst[32];
  sprintf(dst, "short");
}

void sprintf_exact_fit_Good() {
  char dst[6];
  sprintf(dst, "hello");
}

void sprintf_literal_too_long_Bad() {
  char dst[8];
  sprintf(dst, "this literal does not fit");
}

void sprintf_off_by_one_Bad() {
  char dst[5];
  sprintf(dst, "hello");
}

void sprintf_percent_literal_Bad() {
  char dst[2];
  sprintf(dst, "a%%b");
}

void sprintf_around_conversion_Bad(const char* s) {
  char dst[8];
  sprintf(dst, "prefix %s suffix", s);
}

// the conversion may produce nothing, so the format on its own says nothing about the
// length and a destination of any size is accepted here
void sprintf_conversion_only_Good(const char* s) {
  char dst[2];
  sprintf(dst, "%s", s);
}

void vsprintf_literal_too_long_Bad(const char* fmt, va_list ap) {
  char dst[8];
  vsprintf(dst, "this literal does not fit", ap);
}

// a format ends at its first null character, whatever the string literal holds after it
void sprintf_embedded_nul_Good() {
  char dst[2];
  sprintf(dst, "x\0yyyyyyyyyyyyyyyy");
}

void sprintf_initial_nul_Good() {
  char dst[2];
  sprintf(dst, "\0yyyyyyyyyyyyyyyy");
}

void sprintf_nul_inside_conversion_Good() {
  char dst[2];
  sprintf(dst, "%\0dyyyyyyyyyyyyyyyy");
}

// a wide conversion can stop on an encoding error, after which the text that follows it is not
// written; giving up the text before it as well can only miss an overflow, never invent one
void sprintf_wide_char_then_literal_Good(wchar_t wc) {
  char dst[2];
  sprintf(dst, "%lcabcdefgh", wc);
}

void sprintf_wide_string_then_literal_Good(wchar_t* ws) {
  char dst[2];
  sprintf(dst, "%lsabcdefgh", ws);
}

// a long modifier on a narrow conversion is not a wide conversion
void sprintf_long_int_then_literal_Bad(long v) {
  char dst[2];
  sprintf(dst, "%ldabcdefgh", v);
}

// an oversized width may stop output before the following literal text is written
void sprintf_oversize_width_Good() {
  char dst[2];
  sprintf(dst, "%2147483648dabcdefgh", 0);
}

void sprintf_oversize_precision_Good() {
  char dst[2];
  sprintf(dst, "%.2147483648fabcdefgh", 0.0);
}

// a width taken from an argument can be out of range in the same way
// FN: refusing all argument widths also misses this valid overflowing call
void FN_sprintf_star_width_Bad() {
  char dst[2];
  sprintf(dst, "%*dabcdefgh", 5, 0);
}

// a width that is an int is not rejected, so the text after the conversion is still counted
void sprintf_width_then_literal_Bad() {
  char dst[2];
  sprintf(dst, "%2147483647dabcdefgh", 0);
}

// the return value is negative on an encoding error, so this branch is reachable
void sprintf_negative_return_Bad(wchar_t wc) {
  char dst[32];
  int n = sprintf(dst, "%lc", wc);
  if (n < 0) {
    char local[2];
    local[8] = 'x';
  }
}

extern int* opaque_int_pointer(void);
extern char* opaque_char_pointer(void);

// c_strlen of an unknown destination must not replace the contents of an unknown integer cell
void sprintf_unknown_negative_write_Bad(void) {
  int* p = opaque_int_pointer();
  char* dst = opaque_char_pointer();
  *p = -1;
  sprintf(dst, "abc");
  if (*p < 0) {
    char local[2];
    local[8] = 0;
  }
}
