#include "../../cpp_lib/syscalls.h"

void main(){
    char* str = (char*)"Hello, World 0!\n";
    while(1){
        print(str);
        yield();
    }
}