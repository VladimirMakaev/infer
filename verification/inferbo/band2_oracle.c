#include <stdio.h>
#include <stdint.h>
int main(void){ for(int c=0;c<2;c++) for(int d=0;d<2;d++){
  volatile int cond=c; volatile int64_t mask=-(int64_t)(cond!=0), val=d?-3:-2; int64_t i=mask&val;
  printf("cond=%d d=%d: mask=%lld val=%lld i=%lld  i>=-8:%d  index=%lld into int a[7]%s\n",
    c,d,(long long)mask,(long long)val,(long long)i,i>=-8,(long long)(i+8), (i+8)>6?"  <-- OUT OF BOUNDS":""); }
  return 0; }
