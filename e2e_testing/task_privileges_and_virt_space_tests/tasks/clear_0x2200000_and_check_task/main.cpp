#include "../../../../cpp_lib/syscalls.h"

void main(){
    volatile unsigned char* pByte = (unsigned char*)0x2200000;
    *pByte = 0;
    while(1){
        if(*pByte != 0){
            e2eTestingLog(1000);
        }
        else{
            e2eTestingLog(1001);
        }
        yield();
    }
}