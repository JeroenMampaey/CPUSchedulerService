#include "loopback_network_interface.h"

#include "../global_resources/screen.h"
#include "../../cpp_lib/atomic.h"
#include "../cpu_core/cpu_core.h"

void handleLoopbackInterfaceInterrupt(unsigned int interruptParam, unsigned int eax){
    LoopbackNetworkInterface* pLoopbackNetworkInterface = (LoopbackNetworkInterface*)interruptParam;

    Callable<Pair<unsigned char*, unsigned int>>* pPacketHandler = pLoopbackNetworkInterface->pPacketHandler;
    Pair<unsigned char*, unsigned int> readBuffer;
    while((readBuffer = pLoopbackNetworkInterface->getReadBuffer()).first != nullptr){
        if(pPacketHandler!=nullptr){
            pPacketHandler->call({readBuffer.first, readBuffer.second});
        }

        pLoopbackNetworkInterface->finishReadBuffer();
    }
}

LoopbackNetworkInterface::LoopbackNetworkInterface(InterruptType interruptLine, InterruptTrigger interruptTrigger, unsigned char mac[6])
    :
    interruptLine(interruptLine),
    interruptTrigger(interruptTrigger),
    pPacketHandler(nullptr),
    interruptsEnabled(false)
{
    for(int i = 0; i < 6; i++){
        this->mac[i] = mac[i];
    }

    for(int i = 0; i < NUM_LOOPBACK_RX_AND_TX_BUFFERS; i++){
        rxAndTxDescs[i].isFull = false;
        rxAndTxDescs[i].length = 0;
    }
}

unsigned char* LoopbackNetworkInterface::getMac(){
    return mac;
}

void LoopbackNetworkInterface::registerPacketHandler(Callable<Pair<unsigned char*, unsigned int>>* pNewPacketHandler){
    atomicStore((unsigned int*)&pPacketHandler, (unsigned int)pNewPacketHandler);

    if(!interruptsEnabled){
        // Network card interrupts will be enabled on cpu core 0
        CpuCore* pCpuCore = CpuCore::getCpuCore(0);
        if(pCpuCore==nullptr){
            Screen* pScreen = Screen::getScreen();
            pScreen->printk((char*)"Failed to get cpu core 0 to enable network card interrupts\n");
            while(true);
        }

        pCpuCore->getInterruptHandlerManager()->setInterruptHandlerParam(interruptLine, (unsigned int)this);
        pCpuCore->getInterruptHandlerManager()->setInterruptHandler(interruptLine, handleLoopbackInterfaceInterrupt);

        interruptsEnabled = true;
    }
}

unsigned char* LoopbackNetworkInterface::getWriteBuffer(){
    if(rxAndTxDescs[currentTx].isFull){
        return nullptr;
    }

    return rxAndTxBuffers+RX_BUFFER_SIZE*currentTx;
}

void LoopbackNetworkInterface::finishWriteBuffer(unsigned int length, bool isIpv4Packet){
    if(length < ETHERNET_SIMPLE_HEADER_SIZE){
        return;
    }

    if(length > ETHERNET_MTU+ETHERNET_SIMPLE_HEADER_SIZE){
        return;
    }

    if(rxAndTxDescs[currentTx].isFull){
        return;
    }

    if(isIpv4Packet){
        // If ipv4 packet, we need to calculate the checksum
        unsigned char* data = rxAndTxBuffers+RX_BUFFER_SIZE*currentTx;

        if(length < ETHERNET_SIMPLE_HEADER_SIZE+IPV4_MINIMAL_HEADER_SIZE){
            return;
        }

        unsigned char* ipHeader = data + ETHERNET_SIMPLE_HEADER_SIZE;
        unsigned int headerLength = (ipHeader[0] & 0x0F) * 4; // IHL field specifies the number of 4-byte words

        if(headerLength < IPV4_MINIMAL_HEADER_SIZE){
            return;
        }

        if(length < ETHERNET_SIMPLE_HEADER_SIZE+headerLength){
            return;
        }

        // Set checksum field to 0 before calculation
        ipHeader[10] = 0;
        ipHeader[11] = 0;

        unsigned int checksum = 0;
        for (unsigned int i = 0; i < headerLength; i += 2) {
            checksum += (ipHeader[i] << 8) + ipHeader[i + 1];
            if (checksum & 0xFFFF0000) { // If overflow, wrap around
                checksum = (checksum & 0xFFFF) + (checksum >> 16);
            }
        }

        checksum = ~checksum; // One's complement
        ipHeader[10] = static_cast<unsigned char>(checksum >> 8);
        ipHeader[11] = static_cast<unsigned char>(checksum & 0xFF);
    }

    rxAndTxDescs[currentTx].length = length;
    rxAndTxDescs[currentTx].isFull = true;

    currentTx = (currentTx+1)%NUM_LOOPBACK_RX_AND_TX_BUFFERS;
    
    interruptTrigger();
}

Pair<unsigned char*, unsigned int> LoopbackNetworkInterface::getReadBuffer(){
    if(!rxAndTxDescs[currentRx].isFull){
        return {nullptr, 0};
    }

    return {rxAndTxBuffers+RX_BUFFER_SIZE*currentRx, rxAndTxDescs[currentRx].length};
}

void LoopbackNetworkInterface::finishReadBuffer(){
    if(!rxAndTxDescs[currentRx].isFull){
        return;
    }

    rxAndTxDescs[currentRx].isFull = false;

    currentRx = (currentRx+1)%NUM_LOOPBACK_RX_AND_TX_BUFFERS;
}