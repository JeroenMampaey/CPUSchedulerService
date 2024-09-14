#include "../../../../cpp_lib/syscalls.h"

// Obviously this won't work with TEST_CLIENT_IP=1, thus make sure to pass correct TEST_CLIENT_IP to compiler
#ifndef TEST_CLIENT_IP
#define TEST_CLIENT_IP 1
#endif

void main(){
    int socketID = openSocket(1000);

    if(socketID != -1){
        unsigned char sendBuffer[100];
        sendBuffer[0] = (unsigned char)(((unsigned int)TEST_CLIENT_IP) & 0xFF);
        sendBuffer[1] = (unsigned char)((((unsigned int)TEST_CLIENT_IP) >> 8) & 0xFF);
        sendBuffer[2] = (unsigned char)((((unsigned int)TEST_CLIENT_IP) >> 16) & 0xFF);
        sendBuffer[3] = (unsigned char)((((unsigned int)TEST_CLIENT_IP) >> 24) & 0xFF);
        unsigned short destinationPort = 10000;
        sendBuffer[4] = destinationPort & 0xFF;
        sendBuffer[5] = (destinationPort >> 8) & 0xFF;
        // Message is "hello world!"
        unsigned short udpLength = 12 + UDP_HEADER_SIZE;
        sendBuffer[6] = udpLength & 0xFF;
        sendBuffer[7] = (udpLength >> 8) & 0xFF;
        for(int i=0; i<UDP_HEADER_SIZE; i++){
            sendBuffer[8+i] = 0;
        }
        sendBuffer[8+UDP_HEADER_SIZE] = 'h';
        sendBuffer[9+UDP_HEADER_SIZE] = 'e';
        sendBuffer[10+UDP_HEADER_SIZE] = 'l';
        sendBuffer[11+UDP_HEADER_SIZE] = 'l';
        sendBuffer[12+UDP_HEADER_SIZE] = 'o';
        sendBuffer[13+UDP_HEADER_SIZE] = ' ';
        sendBuffer[14+UDP_HEADER_SIZE] = 'w';
        sendBuffer[15+UDP_HEADER_SIZE] = 'o';
        sendBuffer[16+UDP_HEADER_SIZE] = 'r';
        sendBuffer[17+UDP_HEADER_SIZE] = 'l';
        sendBuffer[18+UDP_HEADER_SIZE] = 'd';
        sendBuffer[19+UDP_HEADER_SIZE] = '!';

        int indicatorWhenFinished = 0;

        int success = setSendBuffer(socketID, sendBuffer, udpLength + 2*SEND_BUFFER_HEADER_SIZE, &indicatorWhenFinished);
        
        if(success != -1){
            while(indicatorWhenFinished==0){
                yield();
            }
        }

        closeSocket(socketID);

        e2eTestingLog(99);
    }

    while(1){
        yield();
    }
}