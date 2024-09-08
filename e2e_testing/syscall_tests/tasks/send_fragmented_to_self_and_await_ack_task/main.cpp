#include "../../../../cpp_lib/syscalls.h"

// Obviously this won't work with TEST_CLIENT_IP=1, thus make sure to pass correct TEST_CLIENT_IP to compiler
#ifndef MY_IP
#define MY_IP 1
#endif

void main(){
    int socketID = openSocket(1000);

    if(socketID!=-1){
        // Expected message is "ACK"
        unsigned char receiveBuffer[3+2*RECEIVE_BUFFER_HEADER_SIZE];
        setReceiveBuffer(socketID, receiveBuffer, 3+2*RECEIVE_BUFFER_HEADER_SIZE);

        unsigned int dataLength = 5000;
        unsigned char sendBuffer[dataLength + SEND_BUFFER_HEADER_SIZE + UDP_HEADER_SIZE];
        sendBuffer[0] = (unsigned char)(((unsigned int)MY_IP) & 0xFF);
        sendBuffer[1] = (unsigned char)((((unsigned int)MY_IP) >> 8) & 0xFF);
        sendBuffer[2] = (unsigned char)((((unsigned int)MY_IP) >> 16) & 0xFF);
        sendBuffer[3] = (unsigned char)((((unsigned int)MY_IP) >> 24) & 0xFF);
        unsigned short destinationPort = 2000;
        sendBuffer[4] = destinationPort & 0xFF;
        sendBuffer[5] = (destinationPort >> 8) & 0xFF;
        unsigned short udpLength = dataLength + UDP_HEADER_SIZE;
        sendBuffer[6] = udpLength & 0xFF;
        sendBuffer[7] = (udpLength >> 8) & 0xFF;
        for(int i=0; i<UDP_HEADER_SIZE; i++){
            sendBuffer[8+i] = 0;
        }
        for(int i=0; i<dataLength; i++){
            sendBuffer[SEND_BUFFER_HEADER_SIZE+UDP_HEADER_SIZE+i] = (unsigned char)(i % 10);
        }

        int indicatorWhenFinished = 0;

        e2eTestingLog(1033);

        int success = setSendBuffer(socketID, sendBuffer, SEND_BUFFER_HEADER_SIZE+udpLength, &indicatorWhenFinished);
        
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

        if(sourceIP!=MY_IP || sourcePort!=2000 || packetSize!=3){
            everyByteCorrect = false;
        }

        for(int i=RECEIVE_BUFFER_HEADER_SIZE; i<RECEIVE_BUFFER_HEADER_SIZE+3; i++){
            if(receiveBuffer[i] != "ACK"[i-RECEIVE_BUFFER_HEADER_SIZE]){
                everyByteCorrect = false;
                break;
            }
        }

        for(int i=RECEIVE_BUFFER_HEADER_SIZE+3; i<3+2*RECEIVE_BUFFER_HEADER_SIZE; i++){
            if(receiveBuffer[i] != 0){
                everyByteCorrect = false;
                break;
            }
        }

        if(everyByteCorrect){
            e2eTestingLog(1036);
        }
        else{
            e2eTestingLog(1037);
        }

        closeSocket(socketID);
    }

    while(1){
        yield();
    }
}