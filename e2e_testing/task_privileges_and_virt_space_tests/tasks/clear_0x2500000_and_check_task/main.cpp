#include "../../../../cpp_lib/syscalls.h"

void main(){
    volatile unsigned char* pByte = (unsigned char*)0x2500000;
    *pByte = 0;
    while(1){
        if(*pByte != 0){
            e2eTestingLog(3000);
        }
        else{
            e2eTestingLog(3001);
        }
        yield();
    }
}