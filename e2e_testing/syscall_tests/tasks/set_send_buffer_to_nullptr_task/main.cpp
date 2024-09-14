#include "../../../../cpp_lib/syscalls.h"

// Obviously this won't work with TEST_CLIENT_IP=1, thus make sure to pass correct TEST_CLIENT_IP to compiler
#ifndef TEST_CLIENT_IP
#define TEST_CLIENT_IP 1
#endif

void main(){
    int socketID = openSocket(1000);

    if(socketID != -1){
        // First, test setting an actual send buffer
        unsigned char sendBuffer[100];
        sendBuffer[0] = (unsigned char)(((unsigned int)TEST_CLIENT_IP) & 0xFF);
        sendBuffer[1] = (unsigned char)((((unsigned int)TEST_CLIENT_IP) >> 8) & 0xFF);
        sendBuffer[2] = (unsigned char)((((unsigned int)TEST_CLIENT_IP) >> 16) & 0xFF);
        sendBuffer[3] = (unsigned char)((((unsigned int)TEST_CLIENT_IP) >> 24) & 0xFF);
        unsigned short destinationPort = 10000;
        sendBuffer[4] = destinationPort & 0xFF;
        sendBuffer[5] = (destinationPort >> 8) & 0xFF;
        unsigned short udpLength = 1 + UDP_HEADER_SIZE;
        sendBuffer[6] = udpLength & 0xFF;
        sendBuffer[7] = (udpLength >> 8) & 0xFF;
        for(int i=0; i<UDP_HEADER_SIZE; i++){
            sendBuffer[8+i] = 0;
        }
        sendBuffer[8+UDP_HEADER_SIZE] = '0';
        int indicatorWhenFinished = 0;
        int success = setSendBuffer(socketID, sendBuffer, udpLength + SEND_BUFFER_HEADER_SIZE, &indicatorWhenFinished);
        
        // Normally, this should succeed
        if(success==-1){
            e2eTestingLog(88);
        }
        else{
            e2eTestingLog(89);
        }
        
        // Next, test setting an actual send buffer again
        success = setSendBuffer(socketID, sendBuffer, udpLength + SEND_BUFFER_HEADER_SIZE, &indicatorWhenFinished);

        // Normally, this should succeed
        if(success==-1){
            e2eTestingLog(90);
        }
        else{
            e2eTestingLog(91);
        }
        
        // Next, test setting a nullptr/zero size/nullptr send buffer
        success = setSendBuffer(socketID, nullptr, 0, nullptr);

        // Normally, this should succeed
        if(success==-1){
            e2eTestingLog(92);
        }
        else{
            e2eTestingLog(93);
        }

        // Next, test setting a nullptr/non-zero size/nullptr send buffer
        success = setSendBuffer(socketID, nullptr, 1, nullptr);

        // Normally, this should fail
        if(success==-1){
            e2eTestingLog(94);
        }
        else{
            e2eTestingLog(95);
        }

        // Next, test setting a nullptr/zero size/non-nullptr send buffer
        success = setSendBuffer(socketID, nullptr, 0, &indicatorWhenFinished);

        // Normally, this should fail
        if(success==-1){
            e2eTestingLog(96);
        }
        else{
            e2eTestingLog(97);
        }

        // Next, test setting a non-nullptr/zero size/nullptr send buffer
        success = setSendBuffer(socketID, sendBuffer, 0, nullptr);

        // Normally, this should fail
        if(success==-1){
            e2eTestingLog(98);
        }
        else{
            e2eTestingLog(99);
        }

        // We could do more combinations here now, but above scenarios make us confident it works correctly

        closeSocket(socketID);
    }

    while(1){
        yield();
    }
}