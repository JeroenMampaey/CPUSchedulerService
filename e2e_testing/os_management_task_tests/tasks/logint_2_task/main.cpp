#include "../../../../cpp_lib/syscalls.h"

void main(){
    while(1){
        e2eTestingLog(2);
        yield();
    }
}