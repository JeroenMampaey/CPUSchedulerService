#include "../../../../cpp_lib/syscalls.h"

// Obviously this won't work with TEST_CLIENT_IP=1, thus make sure to pass correct TEST_CLIENT_IP to compiler
#ifndef TEST_CLIENT_IP
#define TEST_CLIENT_IP 1
#endif

/*
expected_data1 = b'hello world!'
expected_data2 = bytes((ord('0') + (i % 10)) for i in range(3000))
*/
#define DATA1_SIZE 12
#define DATA2_SIZE 3000

void main(){
    int socketID = openSocket(1000);

    if(socketID != -1){
        unsigned int dataLength = 2*SEND_BUFFER_HEADER_SIZE + 2*UDP_HEADER_SIZE + DATA1_SIZE + DATA2_SIZE;
        unsigned char sendBuffer[dataLength];

        // Send first packet
        sendBuffer[0] = (unsigned char)(((unsigned int)TEST_CLIENT_IP) & 0xFF);
        sendBuffer[1] = (unsigned char)((((unsigned int)TEST_CLIENT_IP) >> 8) & 0xFF);
        sendBuffer[2] = (unsigned char)((((unsigned int)TEST_CLIENT_IP) >> 16) & 0xFF);
        sendBuffer[3] = (unsigned char)((((unsigned int)TEST_CLIENT_IP) >> 24) & 0xFF);
        unsigned short destinationPort = 10000;
        sendBuffer[4] = destinationPort & 0xFF;
        sendBuffer[5] = (destinationPort >> 8) & 0xFF;
        unsigned short udpLength = DATA1_SIZE + UDP_HEADER_SIZE;
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

        // Send second packet
        unsigned int offset = SEND_BUFFER_HEADER_SIZE + UDP_HEADER_SIZE + DATA1_SIZE;
        sendBuffer[offset] = (unsigned char)(((unsigned int)TEST_CLIENT_IP) & 0xFF);
        sendBuffer[offset+1] = (unsigned char)((((unsigned int)TEST_CLIENT_IP) >> 8) & 0xFF);
        sendBuffer[offset+2] = (unsigned char)((((unsigned int)TEST_CLIENT_IP) >> 16) & 0xFF);
        sendBuffer[offset+3] = (unsigned char)((((unsigned int)TEST_CLIENT_IP) >> 24) & 0xFF);
        destinationPort = 10000;
        sendBuffer[offset+4] = destinationPort & 0xFF;
        sendBuffer[offset+5] = (destinationPort >> 8) & 0xFF;
        udpLength = DATA2_SIZE + UDP_HEADER_SIZE;
        sendBuffer[offset+6] = udpLength & 0xFF;
        sendBuffer[offset+7] = (udpLength >> 8) & 0xFF;
        for(int i=0; i<UDP_HEADER_SIZE; i++){
            sendBuffer[offset+8+i] = 0;
        }
        for(int i=0; i<DATA2_SIZE; i++){
            sendBuffer[offset+SEND_BUFFER_HEADER_SIZE+UDP_HEADER_SIZE+i] = '0' + (i % 10);
        }

        int indicatorWhenFinished = 0;
        int success = setSendBuffer(socketID, sendBuffer, dataLength, &indicatorWhenFinished);
        
        // Yield and normally next time task is scheduled, everything should have been sent
        yield();

        if(indicatorWhenFinished == 1){
            closeSocket(socketID);
            e2eTestingLog(99);
        }
        else{
            closeSocket(socketID);
            e2eTestingLog(100);
        }
    }

    while(1){
        yield();
    }
}