#include "../../../../cpp_lib/syscalls.h"

void main(){
    __asm__ __volatile__(
        ".intel_syntax noprefix;"
        "cli;"
        ".att_syntax;"
    : : :);

    while(1);
}