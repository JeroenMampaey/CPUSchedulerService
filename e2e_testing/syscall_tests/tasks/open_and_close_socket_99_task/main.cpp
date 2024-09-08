#include "../../../../cpp_lib/syscalls.h"

// Obviously this won't work with TEST_CLIENT_IP=1, thus make sure to pass correct TEST_CLIENT_IP to compiler
#ifndef TEST_CLIENT_IP
#define TEST_CLIENT_IP 1
#endif

#define UNUSED_USELESS_PADDING 80

void main(){
    int socketID = openSocket(99);

    closeSocket(socketID);

    if(socketID != -1){
        e2eTestingLog(998);
    }
    else{
        e2eTestingLog(999);
    }

    while(1){
        yield();
    }
}