#include "../../cpp_lib/coap_server.h"
#include "../../cpp_lib/mem.h"
#include "../../cpp_lib/string.h"
#include "../../cpp_lib/placement_new.h"
#include "../../cpp_lib/syscalls.h"

#define REMOTE_IP "192.168.0.135"
#define REMOTE_PORT 9000

void parseIpAddress(const char* ipString, unsigned char ipOctets[4]) {
    int octetIndex = 3;
    unsigned char currentOctet = 0;
    
    while (*ipString != '\0') {
        if (*ipString >= '0' && *ipString <= '9') {
            currentOctet = currentOctet * 10 + (*ipString - '0');
        } else if (*ipString == '.') {
            ipOctets[octetIndex] = currentOctet;
            octetIndex--;
            currentOctet = 0;
        }
        ipString++;
    }
    
    ipOctets[octetIndex] = currentOctet;
}

void main(){
    // First, we open a UDP socket on port 1000
    int socketID = openSocket(1000);
    
    if(socketID != -1){
        char* messages[] = {(char*)"Hello", (char*)"To", (char*)"The", (char*)"World"};
        unsigned char sendBuffer[1000];
        unsigned int currentOffset = 0;

        for(int i=0; i<4; i++){
            // For each outgoing UDP packet we have a header of 16 bytes
            // - bytes 0-3: destination IP address
            // - bytes 4-5: destination port
            // - bytes 6-7: UDP length
            // - bytes 8-15: zeroes, the OS will use these for the UDP header but that is not our concern
            parseIpAddress(REMOTE_IP, sendBuffer+currentOffset);
            sendBuffer[currentOffset+4] = ((unsigned int)REMOTE_PORT) & 0xFF;
            sendBuffer[currentOffset+5] = (((unsigned int)REMOTE_PORT) >> 8) & 0xFF;
            unsigned short udpLength = strlen(messages[i]) + UDP_HEADER_SIZE;
            sendBuffer[currentOffset+6] = udpLength & 0xFF;
            sendBuffer[currentOffset+7] = (udpLength >> 8) & 0xFF;
            for(int j=0; j<UDP_HEADER_SIZE; j++){
                sendBuffer[currentOffset+8+j] = 0;
            }

            // Then we have the actual message
            for(int j=0; j<strlen(messages[i]); j++){
                sendBuffer[currentOffset+8+UDP_HEADER_SIZE+j] = messages[i][j];
            }

            // Then we move on the next message
            currentOffset += 8 + UDP_HEADER_SIZE + strlen(messages[i]);
        }

        // Now send all 4 packets at once
        int indicatorWhenFinished = 0;
        int success = setSendBuffer(socketID, sendBuffer, currentOffset, &indicatorWhenFinished);

        if(success==-1){
            print((char*)"Failed to send the packets!\n");
        }
        else{
            print((char*)"Sending packets...\n");
            // Wait till indicatorWhenFinished is set to 1, indicating
            // that all packets have been sent over the physical network interface
            while(indicatorWhenFinished==0){
                yield();
            }
            print((char*)"Packets successfully sent!\n");
        }

        // Close the socket
        closeSocket(socketID);
        print((char*)"Socket closed!\n");

    }
    else{
        print((char*)"Failed to open UDP socket on port 1000!\n");
    }

    while(1){
        yield();
    }
}