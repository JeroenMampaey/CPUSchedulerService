#pragma once

#include "../global_resources/physical_network_interface.h"

#define NUM_LOOPBACK_RX_AND_TX_BUFFERS 5

typedef void (*InterruptTrigger)();

typedef struct loopbackRxAndTxDesc{
    unsigned int length;
    bool isFull;
}loopbackRxAndTxDesc;

class LoopbackNetworkInterface : public NetworkInterface{
        friend void handleLoopbackInterfaceInterrupt(unsigned int interruptParam, unsigned int eax);

    private:
        Pair<unsigned char*, unsigned int> getReadBuffer();
        void finishReadBuffer();

        unsigned char mac[6];
        InterruptType interruptLine;
        InterruptTrigger interruptTrigger;

        // One single buffer is sufficient because incoming packets will be handled immediately
        unsigned char rxAndTxBuffers[RX_BUFFER_SIZE*NUM_LOOPBACK_RX_AND_TX_BUFFERS] __attribute__((aligned(8)));
        loopbackRxAndTxDesc rxAndTxDescs[NUM_LOOPBACK_RX_AND_TX_BUFFERS];
        
        unsigned int currentRx = 0;
        unsigned int currentTx = 0;

        bool interruptsEnabled;
        Callable<Pair<unsigned char*, unsigned int>>* pPacketHandler;

    public:
        LoopbackNetworkInterface(InterruptType interruptLine, InterruptTrigger interruptTrigger, unsigned char mac[6]);

        unsigned char* getMac() override;
        
        void registerPacketHandler(Callable<Pair<unsigned char*, unsigned int>>* pNewPacketHandler) override;

        unsigned char* getWriteBuffer() override;
        void finishWriteBuffer(unsigned int length, bool isIpv4Packet) override;

};