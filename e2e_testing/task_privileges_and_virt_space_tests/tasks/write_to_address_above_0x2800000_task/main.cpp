#include "../../../../cpp_lib/syscalls.h"

#ifndef RANDOM
#define RANDOM 10
#endif

void main(){
    e2eTestingLog(RANDOM);
    volatile unsigned char* pRandom = (unsigned char*)RANDOM;
    *pRandom = 0;
    while(1);
}