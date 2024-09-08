#include "../../../../cpp_lib/syscalls.h"

#ifndef RANDOM
#define RANDOM 10
#endif

// Obviously this won't work with TEST_CLIENT_IP=1, thus make sure to pass correct TEST_CLIENT_IP to compiler
#ifndef TEST_CLIENT_IP
#define TEST_CLIENT_IP 1
#endif

void main(){
    int socketID = openSocket(1000);

    if(socketID != -1){
        unsigned char* sendBuffer = (unsigned char*)RANDOM;
        unsigned int udpLength = 100;
        int indicatorWhenFinished = 0;

        int success = setSendBuffer(socketID, sendBuffer, udpLength + 2*SEND_BUFFER_HEADER_SIZE, &indicatorWhenFinished);

        if(success==-1){
            e2eTestingLog(199);
        }
        
        closeSocket(socketID);
    }

    while(1){
        yield();
    }
}