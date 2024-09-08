#pragma once

#include "../../cpp_lib/list.h"
#include "../../cpp_lib/pair.h"

#include "../../cpp_lib/mem.h"
#include "../../cpp_lib/atomic.h"

#define INT_MAX ((unsigned int)2147483647)

// FRAGMENT_TIMEOUT/RTC_FREQUENCY = time in seconds a incomplete fragmented udp packet will not be removed from the buffer
#if E2E_TESTING
#define FRAGMENT_TIMEOUT 10
#else
#define FRAGMENT_TIMEOUT 10
#endif

// Following MACROs are copies from global_resources/network_card.h
// But copying avoids header include dependency on network_card.h (which makes unit this class possible)
#define RX_BUFFER_SIZE 2048
#define ETHERNET_MTU 1500
#define ETHERNET_SIMPLE_HEADER_SIZE 14
#define IPV4_MINIMAL_HEADER_SIZE 20

// FRAGMENT_TIMEOUT/RTC_FREQUENCY = time in seconds for these timeouts
#define UNUSED_ARP_ENTRY_TIMEOUT 15
#define ARP_REQUEST_TIMEOUT 10
#define ARP_ENTRY_TIMEOUT 120
#define ARP_BROADCAST_ON_FAILURE_TIMEOUT 15

enum class PacketType{
    NotForThisMachine,
    ARPReply,
    ARPRequest,
    IPv4Packet,
    UnknownType
};

typedef struct IPv4PacketData{
    unsigned char data[RX_BUFFER_SIZE];
} __attribute__((packed)) IPv4PacketData;

typedef struct IPv4Packet{
    unsigned int sourceIP;
    unsigned int destIP;
    unsigned int dataSize;
    unsigned char protocol;

    bool moreFragments;
    unsigned int fragmentOffset;
    unsigned int identification;
    IPv4Packet* nextFragment;

    unsigned int createdTime;
    
    IPv4PacketData* pData;
} IPv4Packet;

typedef struct ARPEntry{
    unsigned int IP;
    int lastUsed;
    int lastRequested;
    int lastAnswered;

    unsigned char MAC[6];
 } ARPEntry;

enum class IPv4PacketProgress{
    WaitingOnARPReply,
    ARPTableFull,
    SendingFragment,
    Done
};

template<unsigned int numPacketBuffers, unsigned int arpHashTableSize, unsigned int arpHashEntryListSize>
class NetworkStackHandler{
        #ifdef UNIT_TESTING
        friend class NetworkStackHandlerTests;
        #endif
    private:
        PacketType incomingARPPacketHelper(unsigned char* data, unsigned int dataLen){
            if(dataLen < 42) return PacketType::UnknownType;

            unsigned short operation = (((unsigned short)(*(data+20))) << 8) + ((unsigned short)(*(data+21)));
            unsigned short arpProtocolType = (((unsigned short)(*(data+16))) << 8) + ((unsigned short)*(data+17));

            switch(arpProtocolType){
                case 0x0800: // ARP for IPv4
                    if(operation==2){
                        unsigned int senderIP = ((unsigned int)(unsigned char)(*(data+28)) << 24) + ((unsigned int)(unsigned char)(*(data+29)) << 16) + ((unsigned int)(unsigned char)(*(data+30)) << 8) + (unsigned int)(unsigned char)(*(data+31));
                        unsigned int hashedSenderIP = senderIP % arpHashTableSize;

                        for(int i=0; i<arpHashEntryListSize; i++){
                            ARPEntry& arpEntry = arpEntries[hashedSenderIP*arpHashEntryListSize + i];
                            if(arpEntry.IP == senderIP){
                                for(int i=0; i<6; i++){
                                    arpEntry.MAC[i] = *(data+22+i);
                                }
                                arpEntry.lastAnswered = timerCounter;
                                break;
                            }
                        }
                        
                        return PacketType::ARPReply;
                    }
                    else if(operation==1){
                        unsigned int requestedIP = ((unsigned int)(unsigned char)(*(data+38)) << 24) + ((unsigned int)(unsigned char)(*(data+39)) << 16) + ((unsigned int)(unsigned char)(*(data+40)) << 8) + (unsigned int)(unsigned char)(*(data+41));
                        if(requestedIP != myIP) return PacketType::NotForThisMachine;

                        return PacketType::ARPRequest;
                    }
                    
                    return PacketType::UnknownType;
                
                default:
                    return PacketType::UnknownType;
            }
        }
        
        PacketType incomingIPv4PacketHelper(unsigned char* data, unsigned int dataLen){
            if(dataLen<34) return PacketType::UnknownType;

            unsigned short headerLength = (unsigned short)(data[14] & 0xF);
            unsigned char version = (data[14] & 0xF0) >> 4;

            if(version!=4 || headerLength<5) return PacketType::UnknownType;

            unsigned int sourceIP = ((unsigned int)(unsigned char)(*(data+26)) << 24) + ((unsigned int)(unsigned char)(*(data+27)) << 16) + ((unsigned int)(unsigned char)(*(data+28)) << 8) + (unsigned int)(unsigned char)(*(data+29));
            unsigned int destIP = ((unsigned int)(unsigned char)(*(data+30)) << 24) + ((unsigned int)(unsigned char)(*(data+31)) << 16) + ((unsigned int)(unsigned char)(*(data+32)) << 8) + (unsigned int)(unsigned char)(*(data+33));

            // IPv4 packet is not destined for this host
            if(destIP!=myIP && destIP!=0xFFFFFFFF) return PacketType::NotForThisMachine;

            unsigned short totalLength = ((unsigned short)data[16] << 8) + (unsigned short)data[17];
            
            if(totalLength<headerLength*4 || (totalLength+ETHERNET_SIMPLE_HEADER_SIZE)>dataLen) return PacketType::UnknownType;

            unsigned int failed = makeSureUnusedPacketsHeadIsntNullptr();

            if(failed==1) return PacketType::IPv4Packet;

            DoublyLinkedListElement<IPv4Packet>* unusedPacketListElement = unusedPacketsHead;
            unusedPacketsHead = unusedPacketsHead->next;
            unusedPacketListElement->next = nullptr;

            unusedPacketListElement->value.sourceIP = sourceIP;
            unusedPacketListElement->value.destIP = destIP;
            unusedPacketListElement->value.dataSize = (unsigned int)(totalLength-headerLength*4);
            unusedPacketListElement->value.protocol = data[23];

            unusedPacketListElement->value.moreFragments = (data[20] & 0x20) > 0;
            unusedPacketListElement->value.fragmentOffset = (unsigned int)(8*(((unsigned short)(data[20] & 0x1F) << 8) + (unsigned short)data[21]));
            unusedPacketListElement->value.identification = (unsigned int)(((unsigned short)data[18] << 8) + (unsigned short)data[19]);
            unusedPacketListElement->value.nextFragment = nullptr;

            unusedPacketListElement->value.createdTime = timerCounter;

            memCopy(&data[14+headerLength*4], unusedPacketListElement->value.pData->data, totalLength-headerLength*4);

            if(unusedPacketListElement->value.moreFragments || unusedPacketListElement->value.fragmentOffset!=0){
                insertInIncompleteFragmentedDatagramsList(unusedPacketListElement);
            }
            else{
                insertInReadyToReadPacketsList(unusedPacketListElement);
            }

            return PacketType::IPv4Packet;
}

        // Returns 0 if successfull, 1 if not
        int makeSureUnusedPacketsHeadIsntNullptr(){
            if(unusedPacketsHead!=nullptr) return 0;
            
            // All packets are in the ReadyToReadPackets list, no free buffer space available
            if(incompleteFragmentedDatagramsHead==nullptr) return 1;

            // uncompleteFragmentedDatagramsHead!=nullptr
            // For the sake of new packets, we can discard some fragments from incomplete datagrams
            DoublyLinkedListElement<IPv4Packet>* incompleteDatagramFragment = incompleteFragmentedDatagramsHead;
            incompleteFragmentedDatagramsHead = incompleteDatagramFragment->next;
            insertFragmentedPacketsInUnusedPacketsList(incompleteDatagramFragment);
            return 0;
        }

        void insertFragmentedPacketsInUnusedPacketsList(DoublyLinkedListElement<IPv4Packet>* firstFragment){
            if(firstFragment==nullptr) return;
            
            DoublyLinkedListElement<IPv4Packet>* listFirst = firstFragment;
            DoublyLinkedListElement<IPv4Packet>* listLast = nullptr;
            
            while(firstFragment != nullptr){
                listLast = firstFragment;

                IPv4Packet* nextFragmentRawPacket = firstFragment->value.nextFragment;
                DoublyLinkedListElement<IPv4Packet>* nextFragment = nullptr;

                if(nextFragmentRawPacket!=nullptr){
                    unsigned int nextFragmentIndex = ((DoublyLinkedListElement<IPv4Packet>*)nextFragmentRawPacket)-packetListElements;
                    nextFragment = &packetListElements[nextFragmentIndex];
                }

                firstFragment->next = nextFragment;
                firstFragment = nextFragment;
            }

            DoublyLinkedListElement<IPv4Packet>* oldHead = nullptr;
            do{
                oldHead = unusedPacketsHead;
                listLast->next = oldHead;
            // Make sure unusedPacketsHead didn't change in the meantime, otherwise try this all again
            // Not clear how big of an impact on performance this can be
            }while(!conditionalExchange((unsigned int*)&unusedPacketsHead, (unsigned int)oldHead, (unsigned int)listFirst));
        }

        void insertInIncompleteFragmentedDatagramsList(DoublyLinkedListElement<IPv4Packet>* pNewFragment){
            DoublyLinkedListElement<IPv4Packet>** nextListElementPtr = &incompleteFragmentedDatagramsHead;
            while(*nextListElementPtr!=nullptr){
                DoublyLinkedListElement<IPv4Packet>* nextListElement = *nextListElementPtr;

                if(passedTimeSince(nextListElement->value.createdTime) > FRAGMENT_TIMEOUT){
                    // If more than MAX_TIMER_COUNTER timerCounter increments have happened since first fragment was received
                    // Then packet has timed out
                    *nextListElementPtr = nextListElement->next;
                    insertFragmentedPacketsInUnusedPacketsList(nextListElement);
                }
                else if(pNewFragment->value.sourceIP==nextListElement->value.sourceIP
                    && pNewFragment->value.destIP==nextListElement->value.destIP
                    && pNewFragment->value.identification==nextListElement->value.identification){
                    // pNewFragment seems to contain one of the fragments for nextListElement
                    break;
                }
                else{
                    nextListElementPtr = &nextListElement->next;
                }
            }

            if(*nextListElementPtr==nullptr){
                *nextListElementPtr = pNewFragment;
            }
            else{
                // *nextListElementPtr is a list of fragments in which pNewFragment might fit
                bool listIsFullSoFar = true;
                IPv4Packet* previous = nullptr;
                IPv4Packet* current = &(*nextListElementPtr)->value;
                IPv4Packet* newest = &pNewFragment->value;
                unsigned int previousOffset = 0;
                unsigned int previousSize = 0;
                while(current!=nullptr && current->fragmentOffset<newest->fragmentOffset){
                    if(current->fragmentOffset != previousOffset+previousSize){
                        listIsFullSoFar = false;
                    }

                    if(!current->moreFragments){
                        return;
                    }

                    previous = current;
                    previousOffset = current->fragmentOffset;
                    previousSize = current->dataSize;
                    current = current->nextFragment;
                }

                if(previous==nullptr){
                    // newFragment should replace first fragment

                    // newest will point to another fragment => moreFragments should definitely be true
                    if(!newest->moreFragments) return;

                    newest->createdTime = current->createdTime;
                    newest->nextFragment = current;
                    pNewFragment->next = (*nextListElementPtr)->next;
                    *nextListElementPtr = pNewFragment;
                }
                else if(current==nullptr){
                    // newFragment has to be added at the end

                    previous->nextFragment = newest;
                }
                else{
                    // current is not nullptr and previous isn't either
                    // newFragment offset is smaller than current offset but bigger than previous offset

                    // newest will point to another fragment => moreFragments should definitely be true
                    if(!newest->moreFragments) return;

                    newest->nextFragment = current;
                    previous->nextFragment = newest;
                }

                // Now we are here, listIsFullSoFar reflects all packets completeness before the new packet which
                // was added, now question is if newest + all proceding packets are also complete
                current = newest;
                while(listIsFullSoFar && current!=nullptr){
                    if(current->fragmentOffset != previousOffset+previousSize){
                        listIsFullSoFar = false;
                    }

                    previous = current;
                    previousOffset = current->fragmentOffset;
                    previousSize = current->dataSize;
                    current = current->nextFragment;
                }

                if(listIsFullSoFar && current==nullptr && !previous->moreFragments){
                    // Adding newFragment completed the datagram

                    DoublyLinkedListElement<IPv4Packet>* pNewPacket = *nextListElementPtr;
                    *nextListElementPtr = pNewPacket->next;
                    insertInReadyToReadPacketsList(pNewPacket);
                }
            }
        }

        void insertInReadyToReadPacketsList(DoublyLinkedListElement<IPv4Packet>* pNewPacket){
            pNewPacket->next = nullptr;
            pNewPacket->prev = readyToReadPacketsTail;

            if(readyToReadPacketsTail==nullptr){
                readyToReadPacketsHead = pNewPacket;
            }
            else{
                readyToReadPacketsTail->next = pNewPacket;
            }

            readyToReadPacketsTail = pNewPacket;
        }

        unsigned int passedTimeSince(int oldTimerCounter){
            unsigned int timePassed;
            
            if(timerCounter >= oldTimerCounter){
                timePassed = timerCounter - oldTimerCounter;
            }
            else{
                timePassed = (INT_MAX - oldTimerCounter + 1) + timerCounter;
            }

            return timePassed;
        }

        unsigned int sendARPRequest(unsigned int destinationIP, unsigned char* writeBuffer){
            for(int i=0; i<6; i++){
                writeBuffer[i] = 0xFF;
                writeBuffer[6+i] = myMac[i];
            }

            writeBuffer[12] = (unsigned char)(((unsigned short)0x0806) >> 8);
            writeBuffer[13] = (unsigned char)(((unsigned short)0x0806) & 0xFF);

            writeBuffer += ETHERNET_SIMPLE_HEADER_SIZE;
            *writeBuffer = 0x00;
            *(writeBuffer+1) = 0x01;
            *(writeBuffer+2) = 0x08;
            *(writeBuffer+3) = 0x00;
            *(writeBuffer+4) = 0x06;
            *(writeBuffer+5) = 0x04;
            *(writeBuffer+6) = 0x00;
            *(writeBuffer+7) = 0x01;
            for(int i=0; i<6; i++) *(writeBuffer+8+i) = myMac[i];
            *(writeBuffer+14) = (unsigned char)((myIP >> 24) & 0xFF);
            *(writeBuffer+15) = (unsigned char)((myIP >> 16) & 0xFF);
            *(writeBuffer+16) = (unsigned char)((myIP >> 8) & 0xFF);
            *(writeBuffer+17) = (unsigned char)(myIP & 0xFF);
            for(int i=0; i<6; i++) *(writeBuffer+18+i) = 0;
            *(writeBuffer+24) = (unsigned char)((destinationIP >> 24) & 0xFF);
            *(writeBuffer+25) = (unsigned char)((destinationIP >> 16) & 0xFF);
            *(writeBuffer+26) = (unsigned char)((destinationIP >> 8) & 0xFF);
            *(writeBuffer+27) = (unsigned char)(destinationIP & 0xFF);

            return 28+ETHERNET_SIMPLE_HEADER_SIZE;
        }

        IPv4PacketData packetDataBuffers[numPacketBuffers];
        DoublyLinkedListElement<IPv4Packet> packetListElements[numPacketBuffers];

        ARPEntry arpEntries[arpHashTableSize*arpHashEntryListSize];
        
        DoublyLinkedListElement<IPv4Packet>* unusedPacketsHead = nullptr;
        DoublyLinkedListElement<IPv4Packet>* incompleteFragmentedDatagramsHead = nullptr;
        DoublyLinkedListElement<IPv4Packet>* readyToReadPacketsHead = nullptr;
        DoublyLinkedListElement<IPv4Packet>* readyToReadPacketsTail = nullptr;

        unsigned char* myMac;
        unsigned int myIP;
        unsigned int gatewayIP;
        unsigned int networkMask;

        int timerCounter;

        unsigned short identificationCounter;

    public:
        NetworkStackHandler(unsigned char* myMac, unsigned int myIP, unsigned int gatewayIP, unsigned int networkMask)
            :
            myMac(myMac),
            myIP(myIP),
            gatewayIP(gatewayIP),
            networkMask((networkMask>=0 && networkMask<=32) ? networkMask : 24),
            timerCounter(0),
            identificationCounter(0)
        {
            for(int i=0; i<numPacketBuffers; i++){
                if(i!=(numPacketBuffers-1)){
                    packetListElements[i].next = &packetListElements[i+1];
                }
                else{
                    packetListElements[i].next = nullptr;
                }

                packetListElements[i].value.pData = &packetDataBuffers[i];
            }

            for(int i=0; i<arpHashTableSize*arpHashEntryListSize; i++){
                ARPEntry& arpEntry = arpEntries[i];

                arpEntry.IP = 0;
                arpEntry.lastUsed = -1;
                arpEntry.lastRequested = -1;
                arpEntry.lastAnswered = -1;

                for(int j=0; j<6; j++){
                    arpEntry.MAC[j] = 0;
                }
            }

            unusedPacketsHead = &packetListElements[0];
            incompleteFragmentedDatagramsHead = nullptr;
            readyToReadPacketsHead = nullptr;
        }

        PacketType handleIncomingEthernetPacket(unsigned char* data, unsigned int dataLen){
            if(dataLen<14) PacketType::UnknownType;

            bool isForMyMac = true;
            bool isMacBroadcast = true;
            for(int i=0; i<6; i++){
                if(*(data+i) != myMac[i]){
                    isForMyMac = false;
                }

                if(*(data+i) != 0xFF){
                    isMacBroadcast = false;
                }
            }

            if(!isForMyMac && !isMacBroadcast) PacketType::NotForThisMachine;

            unsigned short protocolType = (((unsigned short)*(data+12)) << 8) + ((unsigned short)*(data+13));

            switch(protocolType){
                case 0x0806: // ARP
                    return incomingARPPacketHelper(data, dataLen);
                
                case 0x0800: // IPv4
                    return incomingIPv4PacketHelper(data, dataLen);
                
                default:
                    return PacketType::UnknownType;
            }
        }

        unsigned int handleOutgoingEthernetPacket(unsigned char* destinationMac, unsigned char* data, unsigned int dataLen, unsigned char* writeBuffer){
            if(dataLen > ETHERNET_MTU){
                return 0;
            }

            if(writeBuffer==nullptr){
                return 0;
            }
            
            for(int i=0; i<6; i++){
                writeBuffer[i] = destinationMac[i];
                writeBuffer[6+i] = myMac[i];
            }

            writeBuffer[12] = (unsigned char)(dataLen >> 8);
            writeBuffer[13] = (unsigned char)(dataLen & 0xFF);
            
            memCopy(data, writeBuffer+ETHERNET_SIMPLE_HEADER_SIZE, dataLen);
            
            return dataLen+ETHERNET_SIMPLE_HEADER_SIZE;
        }

        unsigned int handleOutgoingArpReply(unsigned char* incomingARPReq, unsigned char* writeBuffer){
            if(writeBuffer==nullptr){
                return 0;
            }
            
            unsigned char destinationMac[6];
            for(int i=0; i<6; i++) destinationMac[i] = *(incomingARPReq+22+i);
            unsigned int destinationIP = ((unsigned int)(unsigned char)(*(incomingARPReq+28)) << 24) + ((unsigned int)(unsigned char)(*(incomingARPReq+29)) << 16) + ((unsigned int)(unsigned char)(*(incomingARPReq+30)) << 8) + (unsigned int)(unsigned char)(*(incomingARPReq+31));

            for(int i=0; i<6; i++){
                writeBuffer[i] = destinationMac[i];
                writeBuffer[6+i] = myMac[i];
            }
            writeBuffer[12] = (unsigned char)(((unsigned short)0x0806) >> 8);
            writeBuffer[13] = (unsigned char)(((unsigned short)0x0806) & 0xFF);

            writeBuffer += ETHERNET_SIMPLE_HEADER_SIZE;
            *writeBuffer = 0x00;
            *(writeBuffer+1) = 0x01;
            *(writeBuffer+2) = 0x08;
            *(writeBuffer+3) = 0x00;
            *(writeBuffer+4) = 0x06;
            *(writeBuffer+5) = 0x04;
            *(writeBuffer+6) = 0x00;
            *(writeBuffer+7) = 0x02;
            for(int i=0; i<6; i++) *(writeBuffer+8+i) = myMac[i];
            *(writeBuffer+14) = (unsigned char)((myIP >> 24) & 0xFF);
            *(writeBuffer+15) = (unsigned char)((myIP >> 16) & 0xFF);
            *(writeBuffer+16) = (unsigned char)((myIP >> 8) & 0xFF);
            *(writeBuffer+17) = (unsigned char)(myIP & 0xFF);
            for(int i=0; i<6; i++) *(writeBuffer+18+i) = *(destinationMac+i);
            *(writeBuffer+24) = (unsigned char)((destinationIP >> 24) & 0xFF);
            *(writeBuffer+25) = (unsigned char)((destinationIP >> 16) & 0xFF);
            *(writeBuffer+26) = (unsigned char)((destinationIP >> 8) & 0xFF);
            *(writeBuffer+27) = (unsigned char)(destinationIP & 0xFF);

            return 28+ETHERNET_SIMPLE_HEADER_SIZE;
        }

        /* Return type:
            -Pair.first explains why the NetworkStackHandler didn't fully send the packet yet (or IPv4PacketProgress::Done if it did)
            -Pair.second contains the amount of data written in the writeBuffer which should be send
        */
        Pair<IPv4PacketProgress, unsigned int> handleOutgoingIPv4Packet(unsigned int destinationIP, 
            unsigned short sourcePort, 
            unsigned short destinationPort, 
            unsigned char protocol,
            unsigned char* data, 
            unsigned int dataLen, 
            unsigned short& identification, 
            unsigned int& fragmentOffset, 
            unsigned char* writeBuffer)
        {
            if(data==nullptr || writeBuffer==nullptr){
                return {IPv4PacketProgress::Done, 0};
            }
            
            if(fragmentOffset>=dataLen){
                return {IPv4PacketProgress::Done, 0};
            }

            ARPEntry* arpEntry = nullptr;
            unsigned int bitMask = ~0U << (32 - networkMask);
            unsigned int nextHopIP = ((destinationIP & bitMask) == (myIP & bitMask)) ? destinationIP : gatewayIP;
            unsigned int hashedIP = nextHopIP % arpHashTableSize;
            
            for(int i=0; i<arpHashEntryListSize; i++){
                ARPEntry& entry = arpEntries[hashedIP*arpHashEntryListSize + i];
                if(entry.IP == nextHopIP){
                    arpEntry = &entry;
                    break;
                }

                if(arpEntry==nullptr || arpEntry->IP!=0){
                    if(entry.IP==0){
                        arpEntry = &entry;
                    }
                    else if(entry.lastUsed==-1 && passedTimeSince(entry.lastRequested) > UNUSED_ARP_ENTRY_TIMEOUT){
                        arpEntry = &entry;
                    }
                    else if(entry.lastUsed!=-1 && (arpEntry==nullptr || (arpEntry->lastUsed!=-1 && passedTimeSince(entry.lastUsed) > passedTimeSince(arpEntry->lastUsed)))){
                        arpEntry = &entry;
                    }
                }
            }

            if(arpEntry==nullptr){
                return {IPv4PacketProgress::ARPTableFull, 0};
            }

            if(arpEntry->IP!=nextHopIP){
                arpEntry->IP = nextHopIP;
                arpEntry->lastUsed = -1;
                arpEntry->lastRequested = -1;
                arpEntry->lastAnswered = -1;
            }

            if(arpEntry->lastRequested==-1){
                arpEntry->lastRequested = timerCounter;
                unsigned int arpRequestSize = sendARPRequest(nextHopIP, writeBuffer);
                return {IPv4PacketProgress::WaitingOnARPReply, arpRequestSize};
            }

            if(arpEntry->lastAnswered==-1 || passedTimeSince(arpEntry->lastAnswered) > passedTimeSince(arpEntry->lastRequested)){
                // last ARP request has not been answered yet
                
                if(passedTimeSince(arpEntry->lastRequested) > ARP_REQUEST_TIMEOUT && passedTimeSince(arpEntry->lastRequested) <= ARP_BROADCAST_ON_FAILURE_TIMEOUT){
                    // ARP request seems to have failed, thus temporarily broadcast the ethernet packets
                    for(int i=0; i<6; i++){
                        arpEntry->MAC[i] = 0xFF;
                    }
                }
                else if(passedTimeSince(arpEntry->lastRequested) > ARP_BROADCAST_ON_FAILURE_TIMEOUT){
                    arpEntry->lastRequested = timerCounter;
                    unsigned int arpRequestSize = sendARPRequest(nextHopIP, writeBuffer);
                    return {IPv4PacketProgress::WaitingOnARPReply, arpRequestSize};
                }
                else{
                    // Still waiting on ARP reply
                    return {IPv4PacketProgress::WaitingOnARPReply, 0};
                }
            }
            else if(passedTimeSince(arpEntry->lastAnswered) > ARP_ENTRY_TIMEOUT){
                // last ARP request has been answered but the answer was too long ago

                arpEntry->lastRequested = timerCounter;
                unsigned int arpRequestSize = sendARPRequest(nextHopIP, writeBuffer);
                return {IPv4PacketProgress::WaitingOnARPReply, arpRequestSize};
            }

            
            // If here then arpEntry->MAC contains the required MAC address
            arpEntry->lastUsed = timerCounter;

            // Now send IPv4 packet...
            bool lastFragment = (dataLen-fragmentOffset) < (ETHERNET_MTU-IPV4_MINIMAL_HEADER_SIZE);
            unsigned int fragmentSize = lastFragment ? (dataLen-fragmentOffset) : (ETHERNET_MTU-IPV4_MINIMAL_HEADER_SIZE);
            
            if(fragmentOffset==0){
                identification = identificationCounter;
                identificationCounter++;
            }

            // It is possible here that arpEntry->MAC is altered by an interrupt during this read in which case the MAC address 
            // in the writeBuffer will be dead wrong, but I won't bother trying to make this atomic or having checks that MAC isn't 
            // changed in the meantime. If IPv4 packet is lost because of unfortunate timing, so be it.
            for (int i = 0; i < 6; i++) {
                writeBuffer[i] = arpEntry->MAC[i];
                writeBuffer[6 + i] = myMac[i];
            }
            writeBuffer[12] = 0x08;
            writeBuffer[13] = 0x00;

            writeBuffer += ETHERNET_SIMPLE_HEADER_SIZE;    
            writeBuffer[0] = 0x45;
            writeBuffer[1] = 0x00;
            writeBuffer[2] = (IPV4_MINIMAL_HEADER_SIZE + fragmentSize) >> 8;
            writeBuffer[3] = (IPV4_MINIMAL_HEADER_SIZE + fragmentSize) & 0xFF;
            writeBuffer[4] = identification >> 8;
            writeBuffer[5] = identification & 0xFF;
            unsigned short flagsAndFragmentOffset = (lastFragment ?  0 : 0x2000) | (fragmentOffset >> 3);
            writeBuffer[6] = flagsAndFragmentOffset >> 8;
            writeBuffer[7] = flagsAndFragmentOffset & 0xFF;
            writeBuffer[8] = 255;
            writeBuffer[9] = protocol;
            writeBuffer[10] = 0x00;
            writeBuffer[11] = 0x00;
            writeBuffer[12] = myIP >> 24;
            writeBuffer[13] = (myIP >> 16) & 0xFF;
            writeBuffer[14] = (myIP >> 8) & 0xFF;
            writeBuffer[15] = myIP & 0xFF;
            writeBuffer[16] = destinationIP >> 24;
            writeBuffer[17] = (destinationIP >> 16) & 0xFF;
            writeBuffer[18] = (destinationIP >> 8) & 0xFF;
            writeBuffer[19] = destinationIP & 0xFF;

            writeBuffer += IPV4_MINIMAL_HEADER_SIZE;
            memCopy(data + fragmentOffset, writeBuffer, fragmentSize);
            fragmentOffset += fragmentSize;

            if(!lastFragment){
                return {IPv4PacketProgress::SendingFragment, ETHERNET_SIMPLE_HEADER_SIZE+IPV4_MINIMAL_HEADER_SIZE+fragmentSize};
            }
            
            return {IPv4PacketProgress::Done, ETHERNET_SIMPLE_HEADER_SIZE+IPV4_MINIMAL_HEADER_SIZE+fragmentSize};
        }

        IPv4Packet* getLatestIPv4Packet(){
            if(readyToReadPacketsHead==nullptr){
                return nullptr;
            }

            return &readyToReadPacketsHead->value;
        }

        void popLatestIPv4Packet(){
            if(readyToReadPacketsHead==nullptr) return;

            DoublyLinkedListElement<IPv4Packet>* poppedPacket = readyToReadPacketsHead;
            conditionalExchange((unsigned int*)&readyToReadPacketsTail, (unsigned int)poppedPacket, (unsigned int)nullptr);
            DoublyLinkedListElement<IPv4Packet>* newHead = poppedPacket->next;
            if(newHead!=nullptr){
                newHead->prev = nullptr;
            }
            conditionalExchange((unsigned int*)&readyToReadPacketsHead, (unsigned int)poppedPacket, (unsigned int)newHead);
            insertFragmentedPacketsInUnusedPacketsList(poppedPacket);
        }

        void incrementTimerCounter(){
            unsigned int numRemovedDatagrams = 0;

            timerCounter++;

            if(timerCounter < 0){
                timerCounter = 0;
            }
        }
};


/*
    Interrupt safety?

    - IPv4Packet* getLatestIPv4Packet
    - void popLatestIPv4Packet
    - Pair<IPv4PacketProgress, unsigned int> handleOutgoingIPv4Packet
    Are functions that are called by tasks and thus can be interrupted

    - PacketType handleIncomingEthernetPacket
    - void incrementTimerCounter
    Are functions that are called by interrupts on the other hand

    Consequences:
    -It is important to make sure the ReadyToReadPackets list stays consistent, tasks can only 
    read/pop from the tail of the list while interrupts can only add to the head of the list 
    -It is important to make sure the UnusedPacketsList stays consistent, tasks can add to the head while interrupts can remove 
    and add to the head -> tasks have to be carefull
    -It is important to make sure the ARPEntries stay consistent, an interrupt handles ARP replies which will update MAC address 
    and lastAnswered members of an ARP entry
*/