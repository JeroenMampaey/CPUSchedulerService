#include "mem.h"

extern "C" void memoryCopyDwords(unsigned char* from, unsigned char* to, int numberOfDwords);
extern "C" void memoryCopyBytes(unsigned char* from, unsigned char* to, int numberOfBytes);

extern "C" void clearDwords(unsigned char* addres, int numberOfDwords);
extern "C" void clearBytes(unsigned char* addres, int numberOfBytes);

void memCopy(unsigned char* from, unsigned char* to, unsigned int numBytes){
    if(numBytes==0) return;

    for(; (((unsigned int)from) % 4)!=0 && numBytes>0; from++, to++, numBytes--){
        *to = *from;
    }

    unsigned int numDwords = numBytes/4;
    memoryCopyDwords(from, to, numDwords);

    for(int i=numDwords*4; i<numBytes; i++){
        *(to+i) = *(from+i);
    }
}

void memClear(unsigned char* addr, unsigned int numBytes){
    if(numBytes==0) return;
    
    for(; (((unsigned int)addr) % 4)!=0 && numBytes>0; addr++, numBytes--){
        *addr = 0;
    }

    unsigned int numDwords = numBytes/4;
    clearDwords(addr, numDwords);

    for(int i=numDwords*4; i<numBytes; i++){
        *(addr+i) = 0;
    }
}

int memCompare(unsigned char* addr1, unsigned char* addr2, unsigned int numBytes){
    if(numBytes==0) return 0;

    for(; (((unsigned int)addr1) % 4)!=0 && numBytes>0; addr1++, addr2++, numBytes--){
        if(*addr1 != *addr2){
            return *addr1 - *addr2;
        }
    }

    unsigned int numDwords = numBytes/4;
    for(int i=0; i<numDwords; i++){
        if(*(unsigned int*)(addr1+i*4) != *(unsigned int*)(addr2+i*4)){
            return *(unsigned int*)(addr1+i*4) - *(unsigned int*)(addr2+i*4);
        }
    }

    for(int i=numDwords*4; i<numBytes; i++){
        if(*(addr1+i) != *(addr2+i)){
            return *(addr1+i) - *(addr2+i);
        }
    }

    return 0;
}