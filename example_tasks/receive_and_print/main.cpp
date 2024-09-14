#include "../../cpp_lib/coap_server.h"
#include "../../cpp_lib/mem.h"
#include "../../cpp_lib/string.h"
#include "../../cpp_lib/placement_new.h"
#include "../../cpp_lib/syscalls.h"

void main(){
    // First, we open a UDP socket on port 1000
    int socketID = openSocket(1000);
    
    if(socketID != -1){
        // Then register a buffer for packet reception
        unsigned char receiveBuffer[1000];
        setReceiveBuffer(socketID, receiveBuffer, 1000);

        // Now wait for packets to be received
        print((char*)"Waiting for packets...\n");
        while(true){
            unsigned int receiveBufferOffset = 0;

            // Iterate over received packets if there are any
            while(receiveBuffer[6+receiveBufferOffset]!=0 || receiveBuffer[7+receiveBufferOffset]!=0){
                // Get sender IP address, sender port and packet size from the sendbuffer header
                unsigned int sourceIP = (((unsigned int)receiveBuffer[3+receiveBufferOffset]) << 24) + (((unsigned int)receiveBuffer[2+receiveBufferOffset]) << 16) + (((unsigned int)receiveBuffer[1+receiveBufferOffset]) << 8) + ((unsigned int)receiveBuffer[receiveBufferOffset]);
                unsigned short sourcePort = (((unsigned short)receiveBuffer[5+receiveBufferOffset]) << 8) + ((unsigned short)receiveBuffer[4+receiveBufferOffset]);
                unsigned short packetSize = (((unsigned int)receiveBuffer[7+receiveBufferOffset]) << 8) + ((unsigned int)receiveBuffer[6+receiveBufferOffset]);

                // Print IP address
                print((char*)"Received packet from ");
                char octetToString[4];
                intToDecimalString((int)(sourceIP >> 24), octetToString);
                print(octetToString);
                print((char*)".");
                intToDecimalString((int)((sourceIP >> 16) & 0xFF), octetToString);
                print(octetToString);
                print((char*)".");
                intToDecimalString((int)((sourceIP >> 8) & 0xFF), octetToString);
                print(octetToString);
                print((char*)".");
                intToDecimalString((int)(sourceIP & 0xFF), octetToString);
                print(octetToString);
                print((char*)":\n");
                print((char*)"        ");

                // Print packet content
                char packetContent[packetSize+1];
                packetContent[packetSize] = '\0';
                for(int i=0; i<packetSize; i++){
                    packetContent[i] = receiveBuffer[RECEIVE_BUFFER_HEADER_SIZE+i+receiveBufferOffset];
                }
                print(packetContent);
                print((char*)"\n");

                // Adapt the offset for the next packet
                receiveBufferOffset += RECEIVE_BUFFER_HEADER_SIZE + packetSize;
            }

            // If packets were received (<=> receiveBufferOffset > 0), then we can reset the buffer
            if(receiveBufferOffset > 0){
                setReceiveBuffer(socketID, receiveBuffer, 1000);
            }

            yield();
        }
    }
    else{
        print((char*)"Failed to open socket!\n");
    }

    while(true){
        yield();
    }
}