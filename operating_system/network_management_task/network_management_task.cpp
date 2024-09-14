#include "network_management_task.h"
#include "../../cpp_lib/placement_new.h"
#include "../cpu_core/cpu_core.h"
#include "../../cpp_lib/string.h"
#include "../../cpp_lib/syscalls.h"
#include "../global_resources/physical_network_interface.h"
#include "../global_resources/serial_log.h"
#include "../global_resources/rtc_timer.h"
#include "../global_resources/screen.h"
#include "loopback_network_interface.h"
#include "../../cpp_lib/callback.h"

#define UDP_IPV4_PROTOCOL 17
#define UDP_HEADER_SIZE 8

#define PHYINT_NUM_PACKET_BUFFERS 1000
#define PHYINT_ARP_HASH_TABLE_SIZE 10
#define PHYINT_ARP_HASH_ENTRY_LIST_SIZE 5

#define LOINT_NUM_PACKET_BUFFERS 100
#define LOINT_ARP_HASH_TABLE_SIZE 1
#define LOINT_ARP_HASH_ENTRY_LIST_SIZE 1

// TODO: ideally in multi-core environment this function should trigger an IPI to a specific core
// Different cores all handling this interrupt will definitely result concurrency issues 
// for the LoopbackNetworkInterface and its NetworkStackHandler
void loopbackInterfaceInterruptTrigger(){
    __asm__ __volatile__(
        ".intel_syntax noprefix;"
        "int 80;"
        ".att_syntax;"
        ::: "memory");
}

void networkManagementTask(CpuCore* pThisCpuCore, SocketManager* pSocketManager, MemoryManager* pMemoryManager){
    PhysicalNetworkInterface* pPhysicalNetworkInterface = PhysicalNetworkInterface::getPhysicalNetworkInterface();

    // Allocate a NetworkStackHandler for the PhysicalNetworkInterface
    unsigned char* phsyicalNetworkStackHandlerAddr = pMemoryManager->allocate(
        alignof(NetworkStackHandler<PHYINT_NUM_PACKET_BUFFERS, PHYINT_ARP_HASH_TABLE_SIZE, PHYINT_ARP_HASH_ENTRY_LIST_SIZE>), 
        sizeof(NetworkStackHandler<PHYINT_NUM_PACKET_BUFFERS, PHYINT_ARP_HASH_TABLE_SIZE, PHYINT_ARP_HASH_ENTRY_LIST_SIZE>));
    if(phsyicalNetworkStackHandlerAddr == nullptr){
        Screen* pScreen = Screen::getScreen();
        pScreen->printk((char*)"Failed to allocate memory for the NetworkStackHandler\n");
        while(1);
    }
    NetworkStackHandler<PHYINT_NUM_PACKET_BUFFERS, PHYINT_ARP_HASH_TABLE_SIZE, PHYINT_ARP_HASH_ENTRY_LIST_SIZE>* pPhysicalNetworkStackHandler = new(phsyicalNetworkStackHandlerAddr) NetworkStackHandler<PHYINT_NUM_PACKET_BUFFERS, PHYINT_ARP_HASH_TABLE_SIZE, PHYINT_ARP_HASH_ENTRY_LIST_SIZE>( 
        pPhysicalNetworkInterface->getMac(),
        #ifdef THIS_IP
            THIS_IP
        #else
            0
        #endif
        ,
        #ifdef GATEWAY_IP
            GATEWAY_IP
        #else
            0
        #endif
        ,
        #if defined(NETWORK_MASK) && (NETWORK_MASK >= 0) && (NETWORK_MASK < 32)
            NETWORK_MASK
        #else
            24
        #endif
    );

    // Allocate the LoopbackNetworkInterface
    unsigned char loopbackMac[6] = {1, 2, 3, 4, 5, 6};
    unsigned char* loopbackNetworkInterfaceAddr = pMemoryManager->allocate(
        alignof(LoopbackNetworkInterface), 
        sizeof(LoopbackNetworkInterface));
    if(loopbackNetworkInterfaceAddr == nullptr){
        Screen* pScreen = Screen::getScreen();
        pScreen->printk((char*)"Failed to allocate memory for the LoopbackNetworkInterface\n");
        while(1);
    }
    LoopbackNetworkInterface* pLoopbackNetworkInterface = new(loopbackNetworkInterfaceAddr) LoopbackNetworkInterface(
        InterruptType::Int80, loopbackInterfaceInterruptTrigger, loopbackMac);
    
    // Allocate a NetworkStackHandler for the LoopbackNetworkInterface
    unsigned char* loopbackNetworkStackHandlerAddr = pMemoryManager->allocate(
        alignof(NetworkStackHandler<LOINT_NUM_PACKET_BUFFERS, LOINT_ARP_HASH_TABLE_SIZE, LOINT_ARP_HASH_ENTRY_LIST_SIZE>), 
        sizeof(NetworkStackHandler<LOINT_NUM_PACKET_BUFFERS, LOINT_ARP_HASH_TABLE_SIZE, LOINT_ARP_HASH_ENTRY_LIST_SIZE>));
    if(loopbackNetworkStackHandlerAddr == nullptr){
        Screen* pScreen = Screen::getScreen();
        pScreen->printk((char*)"Failed to allocate memory for the LoopbackNetworkStackHandler\n");
        while(1);
    }
    NetworkStackHandler<LOINT_NUM_PACKET_BUFFERS, LOINT_ARP_HASH_TABLE_SIZE, LOINT_ARP_HASH_ENTRY_LIST_SIZE>* pLoopbackNetworkStackHandler = new(loopbackNetworkStackHandlerAddr) NetworkStackHandler<LOINT_NUM_PACKET_BUFFERS, LOINT_ARP_HASH_TABLE_SIZE, LOINT_ARP_HASH_ENTRY_LIST_SIZE>(
        pLoopbackNetworkInterface->getMac(),
        #ifdef THIS_IP
            THIS_IP
        #else
            0
        #endif
        ,
        #ifdef THIS_IP
            THIS_IP
        #else
            0
        #endif
        ,
        32
    );

    #if E2E_TESTING
    SerialLog* pSerialLog = SerialLog::getSerialLog();
    pSerialLog->log((char*)"NetworkStack: initializing...\n");
    #endif

    // Setup packet handler for the physical network interface
    class PhysicalNetworkInterfacePacketHandler : public Callable<Pair<unsigned char*, unsigned int>>{
        private:
            NetworkStackHandler<PHYINT_NUM_PACKET_BUFFERS, PHYINT_ARP_HASH_TABLE_SIZE, PHYINT_ARP_HASH_ENTRY_LIST_SIZE>* pNetworkStackHandler;

        public:
            PhysicalNetworkInterfacePacketHandler(NetworkStackHandler<PHYINT_NUM_PACKET_BUFFERS, PHYINT_ARP_HASH_TABLE_SIZE, PHYINT_ARP_HASH_ENTRY_LIST_SIZE>* pNetworkStackHandler){
                this->pNetworkStackHandler = pNetworkStackHandler;
            }

            void call(Pair<unsigned char*, unsigned int> packet){
                unsigned char* readBuffer = packet.first;
                unsigned int readBufferLen = packet.second;
                
                PacketType packetType = pNetworkStackHandler->handleIncomingEthernetPacket(readBuffer, readBufferLen);

                switch(packetType){
                    // ARP requests can easily be handled so let's just do that immediately in the interrupthandler
                    case PacketType::ARPRequest:
                        {
                            PhysicalNetworkInterface* pPhysicalNetworkInterface = PhysicalNetworkInterface::getPhysicalNetworkInterface();
                            unsigned char* writeBuffer = pPhysicalNetworkInterface->getWriteBuffer();
                            unsigned int outgoingPacketSize = pNetworkStackHandler->handleOutgoingArpReply(readBuffer, writeBuffer);
                            pPhysicalNetworkInterface->finishWriteBuffer(outgoingPacketSize, false);
                        }
                        break;
                }
            }
    };
    unsigned char* physicalNetworkInterfacePacketHandlerAddr = pMemoryManager->allocate(
        alignof(PhysicalNetworkInterfacePacketHandler), 
        sizeof(PhysicalNetworkInterfacePacketHandler));
    if(physicalNetworkInterfacePacketHandlerAddr == nullptr){
        Screen* pScreen = Screen::getScreen();
        pScreen->printk((char*)"Failed to allocate memory for the PhysicalNetworkInterfacePacketHandler\n");
        while(1);
    }
    PhysicalNetworkInterfacePacketHandler* pPhysicalNetworkInterfacePacketHandler = new(physicalNetworkInterfacePacketHandlerAddr) PhysicalNetworkInterfacePacketHandler(pPhysicalNetworkStackHandler);
    pPhysicalNetworkInterface->registerPacketHandler(pPhysicalNetworkInterfacePacketHandler);
    
    // Setup packet handler for the loopback network interface
    class LoopbackNetworkInterfacePacketHandler : public Callable<Pair<unsigned char*, unsigned int>>{
        private:
            NetworkStackHandler<LOINT_NUM_PACKET_BUFFERS, LOINT_ARP_HASH_TABLE_SIZE, LOINT_ARP_HASH_ENTRY_LIST_SIZE>* pNetworkStackHandler;
            LoopbackNetworkInterface* pLoopbackNetworkInterface;

        public:
            LoopbackNetworkInterfacePacketHandler(NetworkStackHandler<LOINT_NUM_PACKET_BUFFERS, LOINT_ARP_HASH_TABLE_SIZE, LOINT_ARP_HASH_ENTRY_LIST_SIZE>* pNetworkStackHandler, LoopbackNetworkInterface* pLoopbackNetworkInterface){
                this->pNetworkStackHandler = pNetworkStackHandler;
                this->pLoopbackNetworkInterface = pLoopbackNetworkInterface;
            }

            void call(Pair<unsigned char*, unsigned int> packet){
                unsigned char* readBuffer = packet.first;
                unsigned int readBufferLen = packet.second;
                
                PacketType packetType = pNetworkStackHandler->handleIncomingEthernetPacket(readBuffer, readBufferLen);

                switch(packetType){
                    // ARP requests can easily be handled so let's just do that immediately in the interrupthandler
                    case PacketType::ARPRequest:
                        {
                            unsigned char* writeBuffer = pLoopbackNetworkInterface->getWriteBuffer();
                            unsigned int outgoingPacketSize = pNetworkStackHandler->handleOutgoingArpReply(readBuffer, writeBuffer);
                            pLoopbackNetworkInterface->finishWriteBuffer(outgoingPacketSize, false);
                        }
                        break;
                }
            }
    };
    unsigned char* loopbackNetworkInterfacePacketHandlerAddr = pMemoryManager->allocate(
        alignof(LoopbackNetworkInterfacePacketHandler), 
        sizeof(LoopbackNetworkInterfacePacketHandler));
    if(loopbackNetworkInterfacePacketHandlerAddr == nullptr){
        Screen* pScreen = Screen::getScreen();
        pScreen->printk((char*)"Failed to allocate memory for the LoopbackNetworkInterfacePacketHandler\n");
        while(1);
    }
    LoopbackNetworkInterfacePacketHandler* pLoopbackNetworkInterfacePacketHandler = new(loopbackNetworkInterfacePacketHandlerAddr) LoopbackNetworkInterfacePacketHandler(pLoopbackNetworkStackHandler, pLoopbackNetworkInterface);
    pLoopbackNetworkInterface->registerPacketHandler(pLoopbackNetworkInterfacePacketHandler);

    RTCTimer* pRTCTimer = RTCTimer::getRTCTimer();
    #if E2E_TESTING
    pRTCTimer->setTimerFrequency(2);
    #else
    pRTCTimer->setTimerFrequency(2);
    #endif

    // Setup the RTCTimer to increment timer counters for the NetworkStackHandlers
    class RTCTimerCallback : public Runnable{
        private:
            NetworkStackHandler<PHYINT_NUM_PACKET_BUFFERS, PHYINT_ARP_HASH_TABLE_SIZE, PHYINT_ARP_HASH_ENTRY_LIST_SIZE>* pPhysicalNetworkStackHandler;
            NetworkStackHandler<LOINT_NUM_PACKET_BUFFERS, LOINT_ARP_HASH_TABLE_SIZE, LOINT_ARP_HASH_ENTRY_LIST_SIZE>* pLoopbackNetworkStackHandler;

        public:
            RTCTimerCallback(
                NetworkStackHandler<PHYINT_NUM_PACKET_BUFFERS, PHYINT_ARP_HASH_TABLE_SIZE, PHYINT_ARP_HASH_ENTRY_LIST_SIZE>* pPhysicalNetworkStackHandler,
                NetworkStackHandler<LOINT_NUM_PACKET_BUFFERS, LOINT_ARP_HASH_TABLE_SIZE, LOINT_ARP_HASH_ENTRY_LIST_SIZE>* pLoopbackNetworkStackHandler
            ){
                this->pPhysicalNetworkStackHandler = pPhysicalNetworkStackHandler;
                this->pLoopbackNetworkStackHandler = pLoopbackNetworkStackHandler;
            }

            void run(){
                pPhysicalNetworkStackHandler->incrementTimerCounter();
                pLoopbackNetworkStackHandler->incrementTimerCounter();
            }
    };
    unsigned char* rtcTimerCallbackAddr = pMemoryManager->allocate(
        alignof(RTCTimerCallback), 
        sizeof(RTCTimerCallback));
    if(rtcTimerCallbackAddr == nullptr){
        Screen* pScreen = Screen::getScreen();
        pScreen->printk((char*)"Failed to allocate memory for the RTCTimerCallback\n");
        while(1);
    }
    RTCTimerCallback* pRTCTimerCallback = new(rtcTimerCallbackAddr) RTCTimerCallback(pPhysicalNetworkStackHandler, pLoopbackNetworkStackHandler);
    pRTCTimer->setTimerCallback(pRTCTimerCallback);

    #if E2E_TESTING
    unsigned char helloWorldDestination[] = {(unsigned char)0xFF,
                                            (unsigned char)0xFF,
                                            (unsigned char)0xFF,
                                            (unsigned char)0xFF,
                                            (unsigned char)0xFF,
                                            (unsigned char)0xFF};
    unsigned char helloWorldEthernet[] = "Hello World!";
    unsigned int helloWorldEthernetLen = strlen((char*)helloWorldEthernet);
    unsigned char* writeBuffer = pPhysicalNetworkInterface->getWriteBuffer();
    unsigned int outgoingPacketSize = pPhysicalNetworkStackHandler->handleOutgoingEthernetPacket(helloWorldDestination, helloWorldEthernet, helloWorldEthernetLen, writeBuffer);
    pPhysicalNetworkInterface->finishWriteBuffer(outgoingPacketSize, false);

    #if defined(TEST_CLIENT_IP)
    unsigned char helloWorldUDP[] = "        Hello World!";
    unsigned int helloWorldUDPLen = strlen((char*)helloWorldUDP);
    helloWorldUDP[0] = 1000 >> 8;
    helloWorldUDP[1] = 1000 & 0xFF;
    helloWorldUDP[2] = 9000 >> 8;
    helloWorldUDP[3] = 9000 & 0xFF;
    helloWorldUDP[4] = helloWorldUDPLen >> 8;
    helloWorldUDP[5] = helloWorldUDPLen & 0xFF;
    helloWorldUDP[6] = 0x00;
    helloWorldUDP[7] = 0x00;
    unsigned short identification = 0;
    unsigned int fragmentOffset = 0;
    unsigned int destinationIP = TEST_CLIENT_IP;
    writeBuffer = pPhysicalNetworkInterface->getWriteBuffer();
    Pair<IPv4PacketProgress, unsigned int> state = pPhysicalNetworkStackHandler->handleOutgoingIPv4Packet(
        destinationIP, 1000, 9000, UDP_IPV4_PROTOCOL, helloWorldUDP, helloWorldUDPLen, identification, fragmentOffset, writeBuffer);
    pPhysicalNetworkInterface->finishWriteBuffer(state.second, false);

    writeBuffer = pPhysicalNetworkInterface->getWriteBuffer();
    while(state.first!=IPv4PacketProgress::Done){
        state = pPhysicalNetworkStackHandler->handleOutgoingIPv4Packet(
            destinationIP, 1000, 9000, UDP_IPV4_PROTOCOL, helloWorldUDP, helloWorldUDPLen, identification, fragmentOffset, writeBuffer);
    }
    pPhysicalNetworkInterface->finishWriteBuffer(state.second, true);

    unsigned int fragmentedUDPLen = (NUM_TX_DESCRIPTORS-1)*(ETHERNET_MTU-IPV4_MINIMAL_HEADER_SIZE)-((ETHERNET_MTU-IPV4_MINIMAL_HEADER_SIZE)/2);
    unsigned char fragmentedUDP[(NUM_TX_DESCRIPTORS-1)*(ETHERNET_MTU-IPV4_MINIMAL_HEADER_SIZE)-((ETHERNET_MTU-IPV4_MINIMAL_HEADER_SIZE)/2)];
    for(int i=0; i<fragmentedUDPLen-UDP_HEADER_SIZE; i++){
        fragmentedUDP[i+UDP_HEADER_SIZE] = '0'+(i%10);
    }
    fragmentedUDP[0] = 2000 >> 8;
    fragmentedUDP[1] = 2000 & 0xFF;
    fragmentedUDP[2] = 9000 >> 8;
    fragmentedUDP[3] = 9000 & 0xFF;
    fragmentedUDP[4] = fragmentedUDPLen >> 8;
    fragmentedUDP[5] = fragmentedUDPLen & 0xFF;
    fragmentedUDP[6] = 0x00;
    fragmentedUDP[7] = 0x00;
    fragmentOffset = 0;
    writeBuffer = pPhysicalNetworkInterface->getWriteBuffer();
    state = pPhysicalNetworkStackHandler->handleOutgoingIPv4Packet(
        destinationIP, 2000, 9000, UDP_IPV4_PROTOCOL, fragmentedUDP, fragmentedUDPLen, identification, fragmentOffset, writeBuffer);
    while(state.first!=IPv4PacketProgress::Done){
        pPhysicalNetworkInterface->finishWriteBuffer(state.second, true);
        writeBuffer = pPhysicalNetworkInterface->getWriteBuffer();
        state = pPhysicalNetworkStackHandler->handleOutgoingIPv4Packet(
            destinationIP, 2000, 9000, UDP_IPV4_PROTOCOL, fragmentedUDP, fragmentedUDPLen, identification, fragmentOffset, writeBuffer);
    }
    pPhysicalNetworkInterface->finishWriteBuffer(state.second, true);
    #endif

    pSerialLog->log((char*)"NetworkStack: initialization done\n");
    #endif

    while(1){
        SocketManager::TransmissionRequestsIterator transmissionRequestsIterator = pSocketManager->getTransmissionRequestsIterator();
        
        while(!transmissionRequestsIterator.isFinished()){
            class HandleTransmissionRequest : public Runnable{
                private:
                    SocketManager* pSocketManager;
                    NetworkStackHandler<PHYINT_NUM_PACKET_BUFFERS, PHYINT_ARP_HASH_TABLE_SIZE, PHYINT_ARP_HASH_ENTRY_LIST_SIZE>* pPhysicalNetworkStackHandler;
                    NetworkStackHandler<LOINT_NUM_PACKET_BUFFERS, LOINT_ARP_HASH_TABLE_SIZE, LOINT_ARP_HASH_ENTRY_LIST_SIZE>* pLoopbackNetworkStackHandler;
                    SocketManager::TransmissionRequestsIterator& transmissionRequestsIterator;
                    PhysicalNetworkInterface* pPhysicalNetworkInterface;
                    LoopbackNetworkInterface* pLoopbackNetworkInterface;

                public:
                    HandleTransmissionRequest(
                        SocketManager* pSocketManager,
                        NetworkStackHandler<PHYINT_NUM_PACKET_BUFFERS, PHYINT_ARP_HASH_TABLE_SIZE, PHYINT_ARP_HASH_ENTRY_LIST_SIZE>* pPhysicalNetworkStackHandler,
                        NetworkStackHandler<LOINT_NUM_PACKET_BUFFERS, LOINT_ARP_HASH_TABLE_SIZE, LOINT_ARP_HASH_ENTRY_LIST_SIZE>* pLoopbackNetworkStackHandler,
                        SocketManager::TransmissionRequestsIterator& transmissionRequestsIterator,
                        PhysicalNetworkInterface* pPhysicalNetworkInterface,
                        LoopbackNetworkInterface* pLoopbackNetworkInterface
                    )
                        :
                        pSocketManager(pSocketManager),
                        pPhysicalNetworkStackHandler(pPhysicalNetworkStackHandler),
                        pLoopbackNetworkStackHandler(pLoopbackNetworkStackHandler),
                        transmissionRequestsIterator(transmissionRequestsIterator),
                        pPhysicalNetworkInterface(pPhysicalNetworkInterface),
                        pLoopbackNetworkInterface(pLoopbackNetworkInterface)
                    {}

                    void run(){
                        OutgoingUDPPacket outgoingUDPPacket = transmissionRequestsIterator->getTop();
                        NetworkInterface* pNetworkInterface = outgoingUDPPacket.destinationIP==
                            #ifdef THIS_IP
                                THIS_IP
                            #else
                                0
                            #endif
                            ? (NetworkInterface*)pLoopbackNetworkInterface : (NetworkInterface*)pPhysicalNetworkInterface;
                        unsigned short identification = outgoingUDPPacket.identification;
                        unsigned int fragmentOffset = outgoingUDPPacket.fragmentOffset;
                        unsigned char* writeBuffer = pNetworkInterface->getWriteBuffer();
                        Pair<IPv4PacketProgress, unsigned int> state;

                        while(writeBuffer!=nullptr){
                            if(outgoingUDPPacket.dataLen==0){
                                break;
                            }

                            if(fragmentOffset==0){
                                unsigned char* udpHeader = outgoingUDPPacket.data;
                                udpHeader[0] = outgoingUDPPacket.sourcePort >> 8;
                                udpHeader[1] = outgoingUDPPacket.sourcePort & 0xFF;
                                udpHeader[2] = outgoingUDPPacket.destinationPort >> 8;
                                udpHeader[3] = outgoingUDPPacket.destinationPort & 0xFF;
                                udpHeader[4] = outgoingUDPPacket.dataLen >> 8;
                                udpHeader[5] = outgoingUDPPacket.dataLen & 0xFF;
                                udpHeader[6] = 0x00;
                                udpHeader[7] = 0x00;
                            }

                            if(pNetworkInterface==pPhysicalNetworkInterface){
                                state = pPhysicalNetworkStackHandler->handleOutgoingIPv4Packet(
                                    outgoingUDPPacket.destinationIP, outgoingUDPPacket.sourcePort, outgoingUDPPacket.destinationPort, UDP_IPV4_PROTOCOL, 
                                    outgoingUDPPacket.data, outgoingUDPPacket.dataLen, identification, fragmentOffset, writeBuffer);    
                            }
                            else if(pNetworkInterface==pLoopbackNetworkInterface){
                                state = pLoopbackNetworkStackHandler->handleOutgoingIPv4Packet(
                                    outgoingUDPPacket.destinationIP, outgoingUDPPacket.sourcePort, outgoingUDPPacket.destinationPort, UDP_IPV4_PROTOCOL, 
                                    outgoingUDPPacket.data, outgoingUDPPacket.dataLen, identification, fragmentOffset, writeBuffer);
                            }
                            
                            if(state.second>0){
                                bool usesIPv4Context = state.first==IPv4PacketProgress::Done || state.first==IPv4PacketProgress::SendingFragment;
                                pNetworkInterface->finishWriteBuffer(state.second, usesIPv4Context);
                                writeBuffer = pNetworkInterface->getWriteBuffer();
                            }

                            if(state.first==IPv4PacketProgress::WaitingOnARPReply || state.first==IPv4PacketProgress::ARPTableFull || state.first==IPv4PacketProgress::Done){
                                break;
                            }
                        }

                        if(outgoingUDPPacket.dataLen==0){
                            // Outgoing UDP packet makes no sense, remove it

                            // Indicate that the transmission request is done
                            transmissionRequestsIterator->indicateAsFinished();
                            pSocketManager->remove(transmissionRequestsIterator);
                        }
                        else if(fragmentOffset>=outgoingUDPPacket.dataLen){
                            // Outgoing UDP packet was done
                            bool txRequestDone = transmissionRequestsIterator->removeTop();
                            if(txRequestDone){
                                transmissionRequestsIterator->indicateAsFinished();
                                pSocketManager->remove(transmissionRequestsIterator);
                            }
                            else{
                                pSocketManager->relocateToEnd(transmissionRequestsIterator);
                            }
                        }
                        else if(writeBuffer==nullptr){
                            // Not possible to send anymore packets, network card buffer is full
                            // Will try again next time
                            transmissionRequestsIterator->updateTop(fragmentOffset, identification);
                        }
                        else{
                            // Outgoing UDP packet was not done yet, network card is waiting for ARP responses
                            transmissionRequestsIterator->updateTop(fragmentOffset, identification);
                            transmissionRequestsIterator.goToNext();
                        }
                    }
            };

            HandleTransmissionRequest handleTranmissionRequest(
                pSocketManager,
                pPhysicalNetworkStackHandler,
                pLoopbackNetworkStackHandler,
                transmissionRequestsIterator,
                pPhysicalNetworkInterface,
                pLoopbackNetworkInterface
            );
            pThisCpuCore->withTaskSwitchingPaused(handleTranmissionRequest);
        }

        class HandleReceivedPacket : public Runnable{
            private:
                SocketManager* pSocketManager;
                IPv4Packet* newPacket;

            public:
                HandleReceivedPacket(SocketManager* pSocketManager, IPv4Packet* newPacket)
                    :
                    pSocketManager(pSocketManager),
                    newPacket(newPacket)
                {}

                void run(){
                    pSocketManager->handleReceivedPacket(newPacket);
                }
        };
        IPv4Packet* newPacket = pPhysicalNetworkStackHandler->getLatestIPv4Packet();
        if(newPacket==nullptr){
            yield();
        }
        else{
            while(newPacket!=nullptr){
                HandleReceivedPacket handleReceivedPacket(
                    pSocketManager,
                    newPacket
                );
                pThisCpuCore->withTaskSwitchingPaused(handleReceivedPacket);

                #if E2E_TESTING
                pSerialLog->log((char*)"NMT: new datagram begin\n");
                while(newPacket!=nullptr){
                    pSerialLog->log((char*)"NMT: packet: ");
                    pSerialLog->log(newPacket->fragmentOffset);
                    pSerialLog->log((char*)" ");
                    if(newPacket->moreFragments){
                        pSerialLog->log((char*)"1");
                    }
                    else{
                        pSerialLog->log((char*)"0");
                    }
                    pSerialLog->log((char*)" ");
                    pSerialLog->log(newPacket->dataSize);
                    pSerialLog->log((char*)"\n");
                    
                    newPacket = newPacket->nextFragment;
                }
                pSerialLog->log((char*)"NMT: new datagram end\n");
                #endif

                pPhysicalNetworkStackHandler->popLatestIPv4Packet();
                newPacket = pPhysicalNetworkStackHandler->getLatestIPv4Packet();
            }
        }

        newPacket = pLoopbackNetworkStackHandler->getLatestIPv4Packet();
        if(newPacket==nullptr){
            yield();
        }
        else{
            while(newPacket!=nullptr){
                HandleReceivedPacket handleReceivedPacket(
                    pSocketManager,
                    newPacket
                );
                pThisCpuCore->withTaskSwitchingPaused(handleReceivedPacket);

                #if E2E_TESTING
                pSerialLog->log((char*)"NMT: new datagram begin\n");
                while(newPacket!=nullptr){
                    pSerialLog->log((char*)"NMT: packet: ");
                    pSerialLog->log(newPacket->fragmentOffset);
                    pSerialLog->log((char*)" ");
                    if(newPacket->moreFragments){
                        pSerialLog->log((char*)"1");
                    }
                    else{
                        pSerialLog->log((char*)"0");
                    }
                    pSerialLog->log((char*)" ");
                    pSerialLog->log(newPacket->dataSize);
                    pSerialLog->log((char*)"\n");
                    
                    newPacket = newPacket->nextFragment;
                }
                pSerialLog->log((char*)"NMT: new datagram end\n");
                #endif

                pLoopbackNetworkStackHandler->popLatestIPv4Packet();
                newPacket = pLoopbackNetworkStackHandler->getLatestIPv4Packet();
            }
        }
    }
}