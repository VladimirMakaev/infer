#include <stdio.h>
#include <stdint.h>
int main(void){
  for (int d=0; d<2; d++){ volatile int64_t x=0, y=d?-3:-2; printf("s1 d=%d: 0 & %lld = %lld\n",d,(long long)y,(long long)(x&y)); }
  for (int c=0;c<2;c++) for (int d=0;d<2;d++){ volatile int64_t x=c?-1:0,y=d?-3:-2; printf("s2 c=%d d=%d: %lld & %lld = %lld\n",c,d,(long long)x,(long long)y,(long long)(x&y)); }
  for (int d=0;d<2;d++){ volatile int64_t x=0,y=d?-3:-2; int64_t i=x&y; printf("s3 d=%d: i=%lld, i>=-8 is %d, index i+8=%lld into int a[7]\n",d,(long long)i,i>=-8,(long long)(i+8)); }
  { volatile int64_t x=-3,y=-2; printf("c3: -3 & -2 = %lld\n",(long long)(x&y)); }
  return 0; }
