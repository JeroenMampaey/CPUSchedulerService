#pragma once

#include "../../cpp_lib/pair.h"
#include "../cpu_core/interrupt_handler_manager.h"
#include "../../cpp_lib/memory_manager.h"
#include "../../cpp_lib/callback.h"


class NetworkInterface{
    public:
        virtual ~NetworkInterface() = default;

        virtual unsigned char* getMac() = 0;

        virtual void registerPacketHandler(Callable<Pair<unsigned char*, unsigned int>>* pPacketHandler) = 0;

        virtual unsigned char* getWriteBuffer() = 0;
        // isIpv4Packet basically just means that IPv4 checksum calculation can be 
        // offloaded to the NetworkInterface (and thus possibly the hardware)
        virtual void finishWriteBuffer(unsigned int length, bool isIpv4Packet) = 0;

        static void operator delete (void *p){
            return;
        }
};

#define NUM_RX_DESCRIPTORS 32
#define NUM_TX_DESCRIPTORS 8

#define RX_BUFFER_SIZE 2048
#define TX_BUFFER_SIZE 16288

#define REQUIRED_RX_BUFFER_SIZE (RX_BUFFER_SIZE*NUM_RX_DESCRIPTORS)
#define REQUIRED_TX_BUFFER_SIZE (TX_BUFFER_SIZE*NUM_TX_DESCRIPTORS)

#define ETHERNET_MTU 1500
#define ETHERNET_SIMPLE_HEADER_SIZE 14
#define IPV4_MINIMAL_HEADER_SIZE 20

typedef struct e1000RxDesc{
    unsigned int addrLow;
    unsigned int addrHigh;
    unsigned short length;
    unsigned short checksum;
    unsigned char status;
    unsigned char errors;
    unsigned short special;
} e1000RxDesc;

typedef struct e1000TxDesc{
    unsigned int dword0;
    unsigned int dword1;
    unsigned int dword2;
    unsigned char status;
    unsigned char byte13;
    unsigned short word7;
} e1000TxDesc;

typedef struct e1000TxDescLegacy{
    unsigned int addrLow;
    unsigned int addrHigh;
    unsigned short length;
    unsigned char cso;
    unsigned char cmd;
    unsigned char status;
    unsigned char css;
    unsigned short special;
} e1000TxDescLegacy;

typedef struct e1000TxDescTCPIPContext{
    unsigned char ipcss;
    unsigned char ipcso;
    unsigned short ipcse;
    unsigned char tucss;
    unsigned char tucso;
    unsigned short tucse;
    unsigned short paylenLow;
    unsigned char paylenHighAndDtyp;
    unsigned char tucmd;
    unsigned char staAndRsv;
    unsigned char hdrlen;
    unsigned short mss;
} e1000TxDescTCPIPContext;

typedef struct e1000TxDescTCPIPData{
    unsigned int addrLow;
    unsigned int addrHigh;
    unsigned short dtalenLow;
    unsigned char dtalenHighAndDtype;
    unsigned char dcmd;
    unsigned char staAndRsv;
    unsigned char popts;
    unsigned short special;
} e1000TxDescTCPIPData;

class PhysicalNetworkInterface : public NetworkInterface{
        friend void handlePhysicalNetworkInterfaceInterrupt(unsigned int interruptParam, unsigned int eax);
        friend class NetworkCardEnableInterrupts;

    private:
        static PhysicalNetworkInterface* pPhysicalNetworkInterface;

        PhysicalNetworkInterface();

        Pair<unsigned char*, unsigned int> getReadBuffer();
        void finishReadBuffer();

        void writeCommand(unsigned short address, unsigned int value);
        unsigned int readCommand(unsigned short address);
        unsigned int readEEPROM(unsigned int addr);

        void rxInit();
        void txInit();

        e1000RxDesc rxDescs[NUM_RX_DESCRIPTORS] __attribute__((aligned(64)));
        e1000TxDesc txDescs[NUM_TX_DESCRIPTORS] __attribute__((aligned(64)));

        unsigned char rxBufferSpace[REQUIRED_RX_BUFFER_SIZE] __attribute__((aligned(8)));
        unsigned char txBufferSpace[REQUIRED_TX_BUFFER_SIZE] __attribute__((aligned(8)));

        unsigned int ioBase;
        bool usingMemMappedRegisters;

        unsigned char mac[6];
        unsigned int interruptLine;

        unsigned int currentRx = 0;
        unsigned int currentTx = 0;

        bool interruptsEnabled;
        Callable<Pair<unsigned char*, unsigned int>>* pPacketHandler;

    public:
        static PhysicalNetworkInterface* getPhysicalNetworkInterface();
        static void initialize(MemoryManager* pMemoryManager);

        unsigned char* getMac() override;

        void registerPacketHandler(Callable<Pair<unsigned char*, unsigned int>>* pNewPacketHandler) override;
        
        unsigned char* getWriteBuffer() override;
        void finishWriteBuffer(unsigned int length, bool isIpv4Packet) override;

        bool usesMemMappedRegisters();
        unsigned int getIOBase();
};