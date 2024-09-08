#include "../../../../cpp_lib/syscalls.h"

// Obviously this won't work with TEST_CLIENT_IP=1, thus make sure to pass correct TEST_CLIENT_IP to compiler
#ifndef MY_IP
#define MY_IP 1
#endif

void main(){
    int socketID = openSocket(2000);

    if(socketID!=-1){
        unsigned char receiveBuffer[5000+2*RECEIVE_BUFFER_HEADER_SIZE];
        setReceiveBuffer(socketID, receiveBuffer, 5000+2*RECEIVE_BUFFER_HEADER_SIZE);

        while(receiveBuffer[6]==0 && receiveBuffer[7]==0){
            yield();
        }

        bool everyByteCorrect = true;

        unsigned int sourceIP = (((unsigned int)receiveBuffer[3]) << 24) + (((unsigned int)receiveBuffer[2]) << 16) + (((unsigned int)receiveBuffer[1]) << 8) + ((unsigned int)receiveBuffer[0]);
        unsigned short sourcePort = (((unsigned short)receiveBuffer[5]) << 8) + ((unsigned short)receiveBuffer[4]);
        unsigned short packetSize = (((unsigned int)receiveBuffer[7]) << 8) + ((unsigned int)receiveBuffer[6]);

        if(sourceIP!=MY_IP || sourcePort!=1000 || packetSize!=5000){
            everyByteCorrect = false;
        }

        for(int i=RECEIVE_BUFFER_HEADER_SIZE; i<RECEIVE_BUFFER_HEADER_SIZE+5000; i++){
            if(receiveBuffer[i] != ((i-RECEIVE_BUFFER_HEADER_SIZE) % 10)){
                everyByteCorrect = false;
                break;
            }
        }

        for(int i=RECEIVE_BUFFER_HEADER_SIZE+5000; i<5000+2*RECEIVE_BUFFER_HEADER_SIZE; i++){
            if(receiveBuffer[i] != 0){
                everyByteCorrect = false;
                break;
            }
        }

        if(everyByteCorrect){
            e2eTestingLog(1034);
        }
        else{
            e2eTestingLog(1035);

            closeSocket(socketID);
            while(1){
                yield();
            }
        }

        // Message to send is "ACK"
        unsigned int dataLength = 3;
        unsigned char sendBuffer[dataLength + SEND_BUFFER_HEADER_SIZE + UDP_HEADER_SIZE];
        sendBuffer[0] = (unsigned char)(((unsigned int)MY_IP) & 0xFF);
        sendBuffer[1] = (unsigned char)((((unsigned int)MY_IP) >> 8) & 0xFF);
        sendBuffer[2] = (unsigned char)((((unsigned int)MY_IP) >> 16) & 0xFF);
        sendBuffer[3] = (unsigned char)((((unsigned int)MY_IP) >> 24) & 0xFF);
        unsigned short destinationPort = 1000;
        sendBuffer[4] = destinationPort & 0xFF;
        sendBuffer[5] = (destinationPort >> 8) & 0xFF;
        unsigned short udpLength = dataLength + UDP_HEADER_SIZE;
        sendBuffer[6] = udpLength & 0xFF;
        sendBuffer[7] = (udpLength >> 8) & 0xFF;
        for(int i=0; i<UDP_HEADER_SIZE; i++){
            sendBuffer[8+i] = 0;
        }
        for(int i=0; i<dataLength; i++){
            sendBuffer[SEND_BUFFER_HEADER_SIZE+UDP_HEADER_SIZE+i] = "ACK"[i];
        }
    
        int indicatorWhenFinished = 0;

        int success = setSendBuffer(socketID, sendBuffer, SEND_BUFFER_HEADER_SIZE+udpLength, &indicatorWhenFinished);
        
        if(success != -1){
            while(indicatorWhenFinished==0){
                yield();
            }
        }

        e2eTestingLog(1038);

        closeSocket(socketID);
    }

    while(1){
        yield();
    }
}