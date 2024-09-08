#include "../../../../cpp_lib/syscalls.h"

// Obviously this won't work with TEST_CLIENT_IP=1, thus make sure to pass correct TEST_CLIENT_IP to compiler
#ifndef TEST_CLIENT_IP
#define TEST_CLIENT_IP 1
#endif

void main(){
    int socketID = openSocket(1000);

    if(socketID != -1){
        // Expected message is "hello world!"
        unsigned char receiveBuffer[12+2*RECEIVE_BUFFER_HEADER_SIZE];
        setReceiveBuffer(socketID, receiveBuffer, 12+2*RECEIVE_BUFFER_HEADER_SIZE);

        while(receiveBuffer[6]==0 && receiveBuffer[7]==0){
            yield();
        }

        bool everyByteCorrect = true;

        unsigned int sourceIP = (((unsigned int)receiveBuffer[3]) << 24) + (((unsigned int)receiveBuffer[2]) << 16) + (((unsigned int)receiveBuffer[1]) << 8) + ((unsigned int)receiveBuffer[0]);
        unsigned short sourcePort = (((unsigned short)receiveBuffer[5]) << 8) + ((unsigned short)receiveBuffer[4]);
        unsigned short packetSize = (((unsigned int)receiveBuffer[7]) << 8) + ((unsigned int)receiveBuffer[6]);

        if(sourceIP!=TEST_CLIENT_IP || sourcePort!=10000 || packetSize!=12){
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
            e2eTestingLog(299);
        }
        else{
            e2eTestingLog(309);
        }

        closeSocket(socketID);
    }

    while(1){
        yield();
    }
}