#include <stdio.h>
#include <stdint.h>
int main(void){ for(int64_t n=2;n<=8;n++)
  printf("n=%2lld: size n-1=%2lld  idx n/2=%lld -> %s   | idx n-1=%lld -> %s\n",
    (long long)n,(long long)(n-1),(long long)(n/2), (n/2>=n-1)?"OUT OF BOUNDS":"ok",
    (long long)(n-1), ((n-1)>=(n-1))?"OUT OF BOUNDS":"ok");
  return 0;}
