#pragma once

#include "../../cpp_lib/pair.h"
#include "../../cpp_lib/list.h"

#include "network_stack_handler.h"
#include "../cpu_core/cpu_core.h"

#define MAX_NUM_SOCKETS_PER_TASK 10
#define NUM_UDP_PORTS 9000
#define MAX_NUM_TRANSMISSION_REQUESTS 15

#define RECEIVE_BUFFER_HEADER_SIZE (4 + 2 + 2)
#define SEND_BUFFER_HEADER_SIZE (4 + 2 + 2)

#define UDP_HEADER_SIZE 8

typedef struct SocketDesc{
    unsigned int isActive;
    unsigned short udpPort;
    volatile unsigned char* receiveBuffer;
    unsigned int receiveBufferSize;
    unsigned char* sendBuffer;
    unsigned int sendBufferSize;
    unsigned short sendBufferIdentification;
    unsigned int sendBufferFragmentOffset;
    int* sendBufferIndicatorWhenFinished;
} SocketDesc;

struct OutgoingUDPPacket{
    unsigned short sourcePort = 0;
    unsigned short destinationPort = 0;
    unsigned int destinationIP = 0;
    unsigned char* data = nullptr;
    unsigned int dataLen = 0;
    unsigned short identification = 0;
    unsigned int fragmentOffset = 0;
};

typedef struct UDPPortState{
    unsigned int isActive;
    unsigned char taskID;
    unsigned char socketID;
} UDPPortState;

class SocketManager{
    public:
        class TransmissionRequest{
            public:
                TransmissionRequest();
                TransmissionRequest(SocketManager* pSocketManager, unsigned short udpPort);

                inline unsigned short getUDPPort(){
                    return udpPort;
                }

                OutgoingUDPPacket getTop();
                void updateTop(unsigned int newFragmentOffset, unsigned short newIdentification);
                // Remove top returns true if transmission request is now empty, 
                // if there is possibly still data to send, it returns false
                bool removeTop();

                void indicateAsFinished();

            private:
                SocketManager* pSocketManager;
                unsigned short udpPort;
        };

        class TransmissionRequestsIterator{
                friend class SocketManager;
            public:
                TransmissionRequestsIterator(SocketManager* pSocketManager);

                bool isFinished();
                void goToNext();

                TransmissionRequest* operator->();
            
            private:
                SocketManager* pSocketManager;
                DoublyLinkedListElement<TransmissionRequest>* currentTransmissionRequest;
        };

        SocketManager();
        
        void closeSocket(unsigned char taskID, unsigned char socketID);
        void closeAllSocketsForTask(unsigned char taskID);
        // Returns -1 for failure, otherwise returns the socketID
        int openSocket(unsigned char taskID, unsigned short udpPort);

        // Returns -1 for failure, otherwise returns 0
        int setReceiveBuffer(unsigned char taskID, unsigned char socketID, unsigned char* newBuffer, unsigned int newBufferSize);
        // Returns -1 for failure, otherwise returns 0
        int setSendBuffer(unsigned char taskID, unsigned char socketID, unsigned char* newBuffer, unsigned int newBufferSize, int* indicatorWhenFinished);

        void handleReceivedPacket(IPv4Packet* packet);
        TransmissionRequestsIterator getTransmissionRequestsIterator();
        // removeTransmissionRequest will first move the iterator to the next element before removing
        // if next element was nullptr, this method will returns false
        void remove(TransmissionRequestsIterator& iterator);
        void relocateToEnd(TransmissionRequestsIterator& iterator);

    private:
        SocketDesc socketDescs[NUM_POSSIBLE_TASKS][MAX_NUM_SOCKETS_PER_TASK];
        UDPPortState udpPortStates[NUM_UDP_PORTS];
        DoublyLinkedListElement<TransmissionRequest> transmissionRequestListElements[MAX_NUM_TRANSMISSION_REQUESTS];
        DoublyLinkedListElement<TransmissionRequest>* transmissionRequestsHead;
        DoublyLinkedListElement<TransmissionRequest>* transmissionRequestsTail;
        DoublyLinkedListElement<TransmissionRequest>* unusedTransmissionRequestsHead;
};