#include "../../../../cpp_lib/syscalls.h"

// Obviously this won't work with TEST_CLIENT_IP=1, thus make sure to pass correct TEST_CLIENT_IP to compiler
#ifndef MY_IP
#define MY_IP 1
#endif

void main(){
    int socket1ID = openSocket(1000);
    int socket2ID = openSocket(2000);

    if(socket1ID!=-1 && socket2ID!=-1){
        // Expected message is "hello world!"
        unsigned char receiveBuffer[12+2*RECEIVE_BUFFER_HEADER_SIZE];
        setReceiveBuffer(socket2ID, receiveBuffer, 12+2*RECEIVE_BUFFER_HEADER_SIZE);

        unsigned char sendBuffer[100];
        sendBuffer[0] = (unsigned char)(((unsigned int)MY_IP) & 0xFF);
        sendBuffer[1] = (unsigned char)((((unsigned int)MY_IP) >> 8) & 0xFF);
        sendBuffer[2] = (unsigned char)((((unsigned int)MY_IP) >> 16) & 0xFF);
        sendBuffer[3] = (unsigned char)((((unsigned int)MY_IP) >> 24) & 0xFF);
        unsigned short destinationPort = 2000;
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

        for(int i=0; i<SEND_BUFFER_HEADER_SIZE; i++){
            sendBuffer[SEND_BUFFER_HEADER_SIZE + i] = 0;
        }

        int indicatorWhenFinished = 0;

        int success = setSendBuffer(socket1ID, sendBuffer, udpLength + 2*SEND_BUFFER_HEADER_SIZE, &indicatorWhenFinished);
        
        if(success != -1){
            while(indicatorWhenFinished==0){
                yield();
            }
        }

        while(receiveBuffer[6]==0 && receiveBuffer[7]==0){
            yield();
        }

        bool everyByteCorrect = true;

        unsigned int sourceIP = (((unsigned int)receiveBuffer[3]) << 24) + (((unsigned int)receiveBuffer[2]) << 16) + (((unsigned int)receiveBuffer[1]) << 8) + ((unsigned int)receiveBuffer[0]);
        unsigned short sourcePort = (((unsigned short)receiveBuffer[5]) << 8) + ((unsigned short)receiveBuffer[4]);
        unsigned short packetSize = (((unsigned int)receiveBuffer[7]) << 8) + ((unsigned int)receiveBuffer[6]);

        if(sourceIP!=MY_IP || sourcePort!=1000 || packetSize!=12){
            everyByteCorrect = false;
        }

        for(int i=RECEIVE_BUFFER_HEADER_SIZE; i<RECEIVE_BUFFER_HEADER_SIZE+12; i++){
            if(receiveBuffer[i] != "hello world!"[i-RECEIVE_BUFFER_HEADER_SIZE]){
                everyByteCorrect = false;
                break;
            }
        }

        for(int i=RECEIVE_BUFFER_HEADER_SIZE+12; i<12+2*RECEIVE_BUFFER_HEADER_SIZE; i++){
            if(receiveBuffer[i] != 0){
                everyByteCorrect = false;
                break;
            }
        }

        if(everyByteCorrect){
            e2eTestingLog(400);
        }
        else{
            e2eTestingLog(409);
        }
    }

    if(socket1ID!=-1){
        closeSocket(socket1ID);
    }

    if(socket2ID!=-1){
        closeSocket(socket2ID);
    }

    while(1){
        yield();
    }
}