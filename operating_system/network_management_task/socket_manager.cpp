#include "socket_manager.h"
#include "../../cpp_lib/mem.h"
#include "../../cpp_lib/atomic.h"

SocketManager::SocketManager(){
    for(int i = 0; i < NUM_POSSIBLE_TASKS; i++){
        for(int j = 0; j < MAX_NUM_SOCKETS_PER_TASK; j++){
            socketDescs[i][j].isActive = 0;
        }
    }

    for(int i = 0; i < NUM_UDP_PORTS; i++){
        udpPortStates[i].isActive = 0;
        udpPortStates[i].socketID = 0;
        udpPortStates[i].taskID = 0;
    }

    transmissionRequestsHead = nullptr;
    transmissionRequestsTail = nullptr;
    unusedTransmissionRequestsHead = &transmissionRequestListElements[0];
    for(int i = 0; i < MAX_NUM_TRANSMISSION_REQUESTS - 1; i++){
        transmissionRequestListElements[i].next = &transmissionRequestListElements[i + 1];
    }
    transmissionRequestListElements[MAX_NUM_TRANSMISSION_REQUESTS - 1].next = nullptr;
}

void SocketManager::closeSocket(unsigned char taskID, unsigned char socketID){
    if(socketID >= MAX_NUM_SOCKETS_PER_TASK){
        return;
    }

    SocketDesc* pSocketDesc = &socketDescs[taskID][socketID];

    if(pSocketDesc->isActive==1){
        atomicStore(&udpPortStates[pSocketDesc->udpPort].isActive, 0);
        atomicStore(&pSocketDesc->isActive, 0);
    }
}

void SocketManager::closeAllSocketsForTask(unsigned char taskID){
    for(int i = 0; i < MAX_NUM_SOCKETS_PER_TASK; i++){
        closeSocket(taskID, i);
    }
}

int SocketManager::openSocket(unsigned char taskID, unsigned short udpPort){
    if(udpPort >= NUM_UDP_PORTS){
        return -1;
    }

    if(udpPortStates[udpPort].isActive==1){
        return -1;
    }

    for(int i = 0; i < MAX_NUM_SOCKETS_PER_TASK; i++){
        if(socketDescs[taskID][i].isActive==0){
            socketDescs[taskID][i].isActive = 1;
            socketDescs[taskID][i].udpPort = udpPort;
            socketDescs[taskID][i].receiveBuffer = nullptr;
            socketDescs[taskID][i].receiveBufferSize = 0;
            socketDescs[taskID][i].sendBuffer = nullptr;
            socketDescs[taskID][i].sendBufferSize = 0;
            socketDescs[taskID][i].sendBufferIdentification = 0;
            socketDescs[taskID][i].sendBufferFragmentOffset = 0;
            socketDescs[taskID][i].sendBufferIndicatorWhenFinished = nullptr;

            udpPortStates[udpPort].isActive = 1;
            udpPortStates[udpPort].taskID = taskID;
            udpPortStates[udpPort].socketID = i;

            return i;
        }
    }

    return -1;
}

int SocketManager::setReceiveBuffer(unsigned char taskID, unsigned char socketID, unsigned char* newBuffer, unsigned int newBufferSize){
    if(socketID >= MAX_NUM_SOCKETS_PER_TASK){
        return -1;
    }
    
    if(newBufferSize < 2*RECEIVE_BUFFER_HEADER_SIZE){
        return -1;
    }
    
    SocketDesc* pSocketDesc = &socketDescs[taskID][socketID];

    if(pSocketDesc->isActive==0){
        return -1;
    }

    pSocketDesc->receiveBuffer = newBuffer;
    pSocketDesc->receiveBufferSize = newBufferSize;

    return 0;
}

int SocketManager::setSendBuffer(unsigned char taskID, unsigned char socketID, unsigned char* newBuffer, unsigned int newBufferSize, int* indicatorWhenFinished){    
    if(socketID >= MAX_NUM_SOCKETS_PER_TASK){
        return -1;
    }

    if(newBufferSize < SEND_BUFFER_HEADER_SIZE+UDP_HEADER_SIZE){
        return -1;
    }
    
    SocketDesc* pSocketDesc = &socketDescs[taskID][socketID];

    if(pSocketDesc->isActive==0){
        return -1;
    }

    pSocketDesc->sendBuffer = newBuffer;
    pSocketDesc->sendBufferSize = newBufferSize;
    pSocketDesc->sendBufferIdentification = 0;
    pSocketDesc->sendBufferFragmentOffset = 0;
    pSocketDesc->sendBufferIndicatorWhenFinished = indicatorWhenFinished;

    // First remove element from unusedTransmissionRequestsList
    DoublyLinkedListElement<TransmissionRequest>* newTransmissionRequest = unusedTransmissionRequestsHead;
    if(newTransmissionRequest==nullptr){
        return -1;
    }
    unusedTransmissionRequestsHead = unusedTransmissionRequestsHead->next;

    // Then configure the value for this element
    newTransmissionRequest->value = TransmissionRequest(this, pSocketDesc->udpPort);

    // Now place this element at the begin of the transmissionRequestsList
    newTransmissionRequest->next = transmissionRequestsHead;
    newTransmissionRequest->prev = nullptr;
    if(transmissionRequestsHead!=nullptr){
        transmissionRequestsHead->prev = newTransmissionRequest;
    }
    else{
        transmissionRequestsTail = newTransmissionRequest;
    }
    transmissionRequestsHead = newTransmissionRequest;

    return 0;
}

void SocketManager::handleReceivedPacket(IPv4Packet* packet){
    if(packet->protocol != 17){
        return;
    }

    // Atleast the first packet should be able to contain a full UDP header
    // Maybe because of fragmentation this is not strictly necessary but still it's too ridiculous
    if(packet->dataSize < UDP_HEADER_SIZE){
        return;
    }

    unsigned short destinationPort = (packet->pData->data[2] << 8) | packet->pData->data[3];

    if(udpPortStates[destinationPort].isActive==0){
        return;
    }

    unsigned char taskID = udpPortStates[destinationPort].taskID;
    unsigned char socketID = udpPortStates[destinationPort].socketID;
    SocketDesc* pSocketDesc = &socketDescs[taskID][socketID];

    unsigned short udpLengthAccordingToHeader = (packet->pData->data[4] << 8) | packet->pData->data[5];

    // Received packet format in receivebuffer:
    // | Source IP | Source Port | Packet Size | Data | Next Source IP | Next Source Port | Next Packet Size | ...
    if(udpLengthAccordingToHeader-UDP_HEADER_SIZE + 2*RECEIVE_BUFFER_HEADER_SIZE > pSocketDesc->receiveBufferSize){
        return;
    }

    unsigned short accumulatedUDPSize = 0;
    IPv4Packet* pCurrentIPv4Packet = packet;
    while(pCurrentIPv4Packet!=nullptr){
        if(accumulatedUDPSize + pCurrentIPv4Packet->dataSize > udpLengthAccordingToHeader){
            return;
        }

        if(pCurrentIPv4Packet==packet){
            memCopy(pCurrentIPv4Packet->pData->data + UDP_HEADER_SIZE, 
                (unsigned char*)(pSocketDesc->receiveBuffer + RECEIVE_BUFFER_HEADER_SIZE), 
                pCurrentIPv4Packet->dataSize - UDP_HEADER_SIZE);
        }
        else{
            memCopy(pCurrentIPv4Packet->pData->data, 
                (unsigned char*)(pSocketDesc->receiveBuffer + RECEIVE_BUFFER_HEADER_SIZE) + 
                (accumulatedUDPSize - UDP_HEADER_SIZE), 
                pCurrentIPv4Packet->dataSize);
        }

        accumulatedUDPSize += pCurrentIPv4Packet->dataSize;
        pCurrentIPv4Packet = pCurrentIPv4Packet->nextFragment;
    }

    if(accumulatedUDPSize != udpLengthAccordingToHeader){
        return;
    }

    unsigned short sourcePort = (packet->pData->data[0] << 8) | packet->pData->data[1];

    pSocketDesc->receiveBuffer[0] = (packet->sourceIP) & 0xFF;
    pSocketDesc->receiveBuffer[1] = ((packet->sourceIP) >> 8) & 0xFF;
    pSocketDesc->receiveBuffer[2] = ((packet->sourceIP) >> 16) & 0xFF;
    pSocketDesc->receiveBuffer[3] = ((packet->sourceIP) >> 24) & 0xFF;
    pSocketDesc->receiveBuffer[4] = sourcePort & 0xFF;
    pSocketDesc->receiveBuffer[5] = (sourcePort >> 8) & 0xFF;
    pSocketDesc->receiveBuffer[6] = (udpLengthAccordingToHeader-UDP_HEADER_SIZE) & 0xFF;
    pSocketDesc->receiveBuffer[7] = ((udpLengthAccordingToHeader-UDP_HEADER_SIZE) >> 8) & 0xFF;

    for(int i=0; i<RECEIVE_BUFFER_HEADER_SIZE; i++){
        pSocketDesc->receiveBuffer[i + RECEIVE_BUFFER_HEADER_SIZE + (udpLengthAccordingToHeader-UDP_HEADER_SIZE)] = 0;
    }

    pSocketDesc->receiveBuffer += RECEIVE_BUFFER_HEADER_SIZE + (udpLengthAccordingToHeader-UDP_HEADER_SIZE);
    pSocketDesc->receiveBufferSize -= RECEIVE_BUFFER_HEADER_SIZE + (udpLengthAccordingToHeader-UDP_HEADER_SIZE);
}

SocketManager::TransmissionRequestsIterator SocketManager::getTransmissionRequestsIterator(){
    return TransmissionRequestsIterator(this);
}

void SocketManager::TransmissionRequest::updateTop(unsigned int newFragmentOffset, unsigned short newIdentification){
    if(pSocketManager->udpPortStates[udpPort].isActive==0){
        return;
    }

    unsigned char taskID = pSocketManager->udpPortStates[udpPort].taskID;
    unsigned char socketID = pSocketManager->udpPortStates[udpPort].socketID;
    SocketDesc* pSocketDesc = &pSocketManager->socketDescs[taskID][socketID];

    if(pSocketDesc->sendBufferSize == 0){
        return;
    }

    pSocketDesc->sendBufferFragmentOffset = newFragmentOffset;
    pSocketDesc->sendBufferIdentification = newIdentification;
}

 bool SocketManager::TransmissionRequest::removeTop(){
    bool removeFullTransmissionRequest = false;

    if(pSocketManager->udpPortStates[udpPort].isActive==0){
        removeFullTransmissionRequest = true;
    }
    else{
        unsigned char taskID = pSocketManager->udpPortStates[udpPort].taskID;
        unsigned char socketID = pSocketManager->udpPortStates[udpPort].socketID;
        SocketDesc* pSocketDesc = &pSocketManager->socketDescs[taskID][socketID];

        if(pSocketDesc->sendBufferSize < 2*(SEND_BUFFER_HEADER_SIZE + UDP_HEADER_SIZE)){
            // It's impossible that there is still another packet in the buffer
            removeFullTransmissionRequest = true;
        }
        else{
            // Sendpacket format in sendbuffer:
            // | Dest IP | Dest Port | Packet Size | 8*0 | Data | Next Dest IP | Next Dest Port | Next Packet Size | ...
            unsigned char* sendBufferHeader = pSocketDesc->sendBuffer;
            unsigned short udpLength = (((unsigned short)sendBufferHeader[7]) << 8) + ((unsigned short)sendBufferHeader[6]);

            if(udpLength < UDP_HEADER_SIZE || pSocketDesc->sendBufferSize < 2*SEND_BUFFER_HEADER_SIZE+UDP_HEADER_SIZE+udpLength){
                // It's impossible that there is still another packet in the buffer
                removeFullTransmissionRequest = true;
            }
            else{
                // Current transmission request has possibly more packets in the sendbuffer, go to next packet
                pSocketDesc->sendBuffer += SEND_BUFFER_HEADER_SIZE + udpLength;
                pSocketDesc->sendBufferSize -= SEND_BUFFER_HEADER_SIZE + udpLength;
                pSocketDesc->sendBufferIdentification = 0;
                pSocketDesc->sendBufferFragmentOffset = 0;
            }
        }
    }

    return removeFullTransmissionRequest;
}

void SocketManager::TransmissionRequest::indicateAsFinished(){
    if(pSocketManager->udpPortStates[udpPort].isActive==0){
        return;
    }

    unsigned char taskID = pSocketManager->udpPortStates[udpPort].taskID;
    unsigned char socketID = pSocketManager->udpPortStates[udpPort].socketID;
    SocketDesc* pSocketDesc = &pSocketManager->socketDescs[taskID][socketID];

    if(pSocketDesc->sendBufferIndicatorWhenFinished!=nullptr){
        *(pSocketDesc->sendBufferIndicatorWhenFinished) = 1;
    }
}

SocketManager::TransmissionRequestsIterator::TransmissionRequestsIterator(SocketManager* pSocketManager)
    :
    pSocketManager(pSocketManager),
    currentTransmissionRequest(pSocketManager->transmissionRequestsHead)
{}

OutgoingUDPPacket SocketManager::TransmissionRequest::getTop(){
    if(pSocketManager->udpPortStates[udpPort].isActive==0){
        return OutgoingUDPPacket();
    }

    unsigned char taskID = pSocketManager->udpPortStates[udpPort].taskID;
    unsigned char socketID = pSocketManager->udpPortStates[udpPort].socketID;
    SocketDesc* pSocketDesc = &pSocketManager->socketDescs[taskID][socketID];

    if(pSocketDesc->sendBufferSize < SEND_BUFFER_HEADER_SIZE + UDP_HEADER_SIZE){
        return OutgoingUDPPacket();
    }

    // Sendpacket format in sendbuffer:
    // | Dest IP | Dest Port | Packet Size | 8*0 | Data | Next Dest IP | Next Dest Port | Next Packet Size | ...
    unsigned char* sendBufferHeader = pSocketDesc->sendBuffer;
    unsigned short udpLength = (((unsigned short)sendBufferHeader[7]) << 8) + ((unsigned short)sendBufferHeader[6]);

    if(udpLength < UDP_HEADER_SIZE || pSocketDesc->sendBufferSize < SEND_BUFFER_HEADER_SIZE + udpLength){
        return OutgoingUDPPacket();
    }

    OutgoingUDPPacket packet;
    packet.sourcePort = udpPort;
    packet.destinationPort = (((unsigned short)sendBufferHeader[5]) << 8) + ((unsigned short)sendBufferHeader[4]);
    packet.destinationIP = (((unsigned int)sendBufferHeader[3]) << 24) + (((unsigned int)sendBufferHeader[2]) << 16) + (((unsigned int)sendBufferHeader[1]) << 8) + ((unsigned int)sendBufferHeader[0]);
    packet.data = sendBufferHeader + SEND_BUFFER_HEADER_SIZE;
    packet.dataLen = udpLength;
    packet.identification = pSocketDesc->sendBufferIdentification;
    packet.fragmentOffset = pSocketDesc->sendBufferFragmentOffset;

    return packet;
}

bool SocketManager::TransmissionRequestsIterator::isFinished(){
    return currentTransmissionRequest==nullptr;
}

void SocketManager::TransmissionRequestsIterator::goToNext(){
    if(currentTransmissionRequest==nullptr){
        return;
    }

    currentTransmissionRequest = currentTransmissionRequest->next;
}

SocketManager::TransmissionRequest* SocketManager::TransmissionRequestsIterator::operator->(){
    if(currentTransmissionRequest==nullptr){
        return nullptr;
    }

    return &(currentTransmissionRequest->value);
}

SocketManager::TransmissionRequest::TransmissionRequest(){
    pSocketManager = nullptr;
    udpPort = 0;
}

SocketManager::TransmissionRequest::TransmissionRequest(SocketManager* pSocketManager, unsigned short udpPort){
    this->pSocketManager = pSocketManager;
    this->udpPort = udpPort;
}

void SocketManager::remove(TransmissionRequestsIterator& iterator){
    if(iterator.currentTransmissionRequest==nullptr){
        return;
    }

    DoublyLinkedListElement<TransmissionRequest>* currentTransmissionRequest = iterator.currentTransmissionRequest;

    // First move iterator already to next element
    iterator.currentTransmissionRequest = currentTransmissionRequest->next;

    // Now, remove the current element
    if(currentTransmissionRequest->prev!=nullptr){
        currentTransmissionRequest->prev->next = currentTransmissionRequest->next;
    }
    else{
        transmissionRequestsHead = currentTransmissionRequest->next;
    }

    if(currentTransmissionRequest->next!=nullptr){
        currentTransmissionRequest->next->prev = currentTransmissionRequest->prev;
    }
    else{
        transmissionRequestsTail = currentTransmissionRequest->prev;
    }

    currentTransmissionRequest->next = unusedTransmissionRequestsHead;
    unusedTransmissionRequestsHead = currentTransmissionRequest;
}

void SocketManager::relocateToEnd(TransmissionRequestsIterator& iterator){
    if(iterator.currentTransmissionRequest==nullptr){
        return;
    }

    // If there is no next element, then we would just move this element to the end and then go to it
    // Which would be the same as doing nothing
    if(iterator.currentTransmissionRequest->next==nullptr){
        return;
    }

    DoublyLinkedListElement<TransmissionRequest>* currentTransmissionRequest = iterator.currentTransmissionRequest;

    // First remove this element from the list and let iterator point to next element
    iterator.currentTransmissionRequest = currentTransmissionRequest->next;
    currentTransmissionRequest->next->prev = currentTransmissionRequest->prev;
    if(currentTransmissionRequest->prev!=nullptr){
        currentTransmissionRequest->prev->next = currentTransmissionRequest->next;
    }
    else{
        transmissionRequestsHead = currentTransmissionRequest->next;
    }

    // Now add this element to the end of the list
    // Tail here is guaranteed to be non-null and cannot be currentTransmissionRequest because we have 
    // at least one next element
    transmissionRequestsTail->next = currentTransmissionRequest;
    currentTransmissionRequest->prev = transmissionRequestsTail;
    currentTransmissionRequest->next = nullptr;
    transmissionRequestsTail = currentTransmissionRequest;
}