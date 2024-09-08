#include "../../../../cpp_lib/syscalls.h"

void main(){
    volatile unsigned char* pByte = (unsigned char*)0x2200000;
    *pByte = 1;
    while(1){
        if(*pByte != 1){
            e2eTestingLog(2000);
        }
        else{
            e2eTestingLog(2001);
        }
        yield();
    }
}