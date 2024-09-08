#include "../../../../cpp_lib/syscalls.h"

void main(){
    e2eTestingLog(1998);

    unsigned int beginTime = getTimerCounter();
    unsigned int newTime;
    while(getTimerCounter() - beginTime < 20*10);

    e2eTestingLog(1999);

    while(true){
        yield();
    }
}