#include "../../../../cpp_lib/syscalls.h"

void main(){
    volatile unsigned char* pByte = (unsigned char*)0x2500000;
    *pByte = 1;
    while(1){
        if(*pByte != 1){
            e2eTestingLog(4000);
        }
        else{
            e2eTestingLog(4001);
        }
        yield();
    }
}