#include "../../../../cpp_lib/syscalls.h"

void main(){
    // Writes to kernel space
    volatile unsigned char* pRandom = (unsigned char*)100;
    *pRandom = 0;
    while(1);
}