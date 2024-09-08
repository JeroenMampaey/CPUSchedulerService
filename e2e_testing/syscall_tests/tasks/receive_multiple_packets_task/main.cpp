#include "../../../../cpp_lib/syscalls.h"

// Obviously this won't work with TEST_CLIENT_IP=1, thus make sure to pass correct TEST_CLIENT_IP to compiler
#ifndef TEST_CLIENT_IP
#define TEST_CLIENT_IP 1
#endif

/*
expected_data1 = b'hello world!'
expected_data2 = bytes((ord('0') + (i % 10)) for i in range(3000))
expected_data3 = "1"*1000
expected_data4 = "2"*2000
*/
#define DATA1_SIZE 12
#define DATA2_SIZE 3000
#define DATA3_SIZE 1000
#define DATA4_SIZE 2000

void main(){
    int socketID = openSocket(1000);

    if(socketID != -1){
        unsigned char receiveBuffer[5*RECEIVE_BUFFER_HEADER_SIZE+DATA1_SIZE + DATA2_SIZE + DATA3_SIZE + DATA4_SIZE];
        setReceiveBuffer(socketID, receiveBuffer, 5*RECEIVE_BUFFER_HEADER_SIZE+DATA1_SIZE + DATA2_SIZE + DATA3_SIZE + DATA4_SIZE);

        bool everyByteCorrect = true;

        // Check if first received packet correct
        while(receiveBuffer[6]==0 && receiveBuffer[7]==0){
            yield();
        }

        unsigned int sourceIP = (((unsigned int)receiveBuffer[3]) << 24) + (((unsigned int)receiveBuffer[2]) << 16) + (((unsigned int)receiveBuffer[1]) << 8) + ((unsigned int)receiveBuffer[0]);
        unsigned short sourcePort = (((unsigned short)receiveBuffer[5]) << 8) + ((unsigned short)receiveBuffer[4]);
        unsigned short packetSize = (((unsigned int)receiveBuffer[7]) << 8) + ((unsigned int)receiveBuffer[6]);

        if(sourceIP!=TEST_CLIENT_IP || sourcePort!=10000 || packetSize!=DATA1_SIZE){
            everyByteCorrect = false;
        }

        for(int i=RECEIVE_BUFFER_HEADER_SIZE; i<RECEIVE_BUFFER_HEADER_SIZE+DATA1_SIZE; i++){
            if(receiveBuffer[i] != "hello world!"[i-RECEIVE_BUFFER_HEADER_SIZE]){
                everyByteCorrect = false;
                break;
            }
        }

        // Check if second received packet correct
        unsigned int offset = RECEIVE_BUFFER_HEADER_SIZE + DATA1_SIZE;
        while(receiveBuffer[offset+6]==0 && receiveBuffer[offset+7]==0){
            yield();
        }

        sourceIP = (((unsigned int)receiveBuffer[offset+3]) << 24) + (((unsigned int)receiveBuffer[offset+2]) << 16) + (((unsigned int)receiveBuffer[offset+1]) << 8) + ((unsigned int)receiveBuffer[offset+0]);
        sourcePort = (((unsigned short)receiveBuffer[offset+5]) << 8) + ((unsigned short)receiveBuffer[offset+4]);
        packetSize = (((unsigned int)receiveBuffer[offset+7]) << 8) + ((unsigned int)receiveBuffer[offset+6]);

        if(sourceIP!=TEST_CLIENT_IP || sourcePort!=10000 || packetSize!=DATA2_SIZE){
            everyByteCorrect = false;
        }

        for(int i=0; i<DATA2_SIZE; i++){
            if(receiveBuffer[offset+RECEIVE_BUFFER_HEADER_SIZE+i] != ('0'+(i % 10))){
                everyByteCorrect = false;
                break;
            }
        }

        // Check if third received packet correct
        offset += RECEIVE_BUFFER_HEADER_SIZE + DATA2_SIZE;
        while(receiveBuffer[offset+6]==0 && receiveBuffer[offset+7]==0){
            yield();
        }

        sourceIP = (((unsigned int)receiveBuffer[offset+3]) << 24) + (((unsigned int)receiveBuffer[offset+2]) << 16) + (((unsigned int)receiveBuffer[offset+1]) << 8) + ((unsigned int)receiveBuffer[offset+0]);
        sourcePort = (((unsigned short)receiveBuffer[offset+5]) << 8) + ((unsigned short)receiveBuffer[offset+4]);
        packetSize = (((unsigned int)receiveBuffer[offset+7]) << 8) + ((unsigned int)receiveBuffer[offset+6]);

        if(sourceIP!=TEST_CLIENT_IP || sourcePort!=10000 || packetSize!=DATA3_SIZE){
            everyByteCorrect = false;
        }

        for(int i=0; i<DATA3_SIZE; i++){
            if(receiveBuffer[offset+RECEIVE_BUFFER_HEADER_SIZE+i] != '1'){
                everyByteCorrect = false;
                break;
            }
        }

        // Check if fourth received packet correct
        offset += RECEIVE_BUFFER_HEADER_SIZE + DATA3_SIZE;
        while(receiveBuffer[offset+6]==0 && receiveBuffer[offset+7]==0){
            yield();
        }

        sourceIP = (((unsigned int)receiveBuffer[offset+3]) << 24) + (((unsigned int)receiveBuffer[offset+2]) << 16) + (((unsigned int)receiveBuffer[offset+1]) << 8) + ((unsigned int)receiveBuffer[offset+0]);
        sourcePort = (((unsigned short)receiveBuffer[offset+5]) << 8) + ((unsigned short)receiveBuffer[offset+4]);
        packetSize = (((unsigned int)receiveBuffer[offset+7]) << 8) + ((unsigned int)receiveBuffer[offset+6]);

        if(sourceIP!=TEST_CLIENT_IP || sourcePort!=10000 || packetSize!=DATA4_SIZE){
            everyByteCorrect = false;
        }

        for(int i=0; i<DATA4_SIZE; i++){
            if(receiveBuffer[offset+RECEIVE_BUFFER_HEADER_SIZE+i] != '2'){
                everyByteCorrect = false;
                break;
            }
        }

        // Check if final receive buffer header are zeroes
        offset += RECEIVE_BUFFER_HEADER_SIZE + DATA4_SIZE;
        for(int i=offset; i<5*RECEIVE_BUFFER_HEADER_SIZE+DATA1_SIZE + DATA2_SIZE + DATA3_SIZE + DATA4_SIZE; i++){
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