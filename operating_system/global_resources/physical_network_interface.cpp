#include "physical_network_interface.h"
#include "io/ports.h"
#include "io/pci.h"
#include "../../cpp_lib/atomic.h"

#include "../../cpp_lib/placement_new.h"

#include "serial_log.h"
#include "screen.h"

#include "../cpu_core/cpu_core.h"

// A lot of this is from:
// https://wiki.osdev.org/Intel_Ethernet_i217

//https://www.intel.com/content/dam/doc/manual/pci-pci-x-family-gbe-controllers-software-dev-manual.pdf page 95
#define INTEL_82540EM_DEVICE_ID 0x100E
#define INTEL_PCI_VENDOR_ID 0x8086

//https://www.intel.com/content/dam/doc/manual/pci-pci-x-family-gbe-controllers-software-dev-manual.pdf page 71-72-73
#define ETHERNET_ADAPTER_CLASS_CODE 0x20000
#define BAR0_OFFSET 0x10
#define STATUS_AND_COMMAND_REGISTER_OFFSET 0x4
#define CLASS_CODE_AND_REVISION_OFFSET 0x8
#define RESERVED_BAR_OFFSET 0x20
#define MEM_BAR_OFFSET 0x10
#define IO_BAR_OFFSET 0x18
#define MAX_LAT_MIN_GRANT_INT_PIN_INT_LINE_OFFSET 0x3C

//https://www.intel.com/content/dam/doc/manual/pci-pci-x-family-gbe-controllers-software-dev-manual.pdf page 219
#define DEVICE_CONTROL_REGISTER 0x0
#define EEPROM_READ_REGISTER 0x14
#define MULTICAST_TABLE_ARRAY_FIRST_REGISTER 0x5200
#define INTERRUPT_MASK_SR_REGISTER 0xD0
#define INTERRUPT_MASK_C_REGISTER 0xD8
#define INTERRUPT_CAUSE_READ_REGISTER 0xC0
#define WAKEUP_FILTER_CONTROL_REGISTER 0x5808
#define IPv4AT_ENTRY1 0x5840

// From the OSdev https://wiki.osdev.org/Intel_Ethernet_i217
#define RCTL_EN                         (1 << 1)
#define RCTL_SBP                        (1 << 2)
#define RCTL_UPE                        (1 << 3)
#define RCTL_MPE                        (1 << 4)
#define RCTL_LPE                        (1 << 5)
#define RCTL_LBM_NONE                   (0 << 6)
#define RCTL_LBM_PHY                    (3 << 6)
#define RTCL_RDMTS_HALF                 (0 << 8)
#define RTCL_RDMTS_QUARTER              (1 << 8)
#define RTCL_RDMTS_EIGHTH               (2 << 8)
#define RCTL_MO_36                      (0 << 12)
#define RCTL_MO_35                      (1 << 12)
#define RCTL_MO_34                      (2 << 12)
#define RCTL_MO_32                      (3 << 12)
#define RCTL_BAM                        (1 << 15)
#define RCTL_VFE                        (1 << 18)
#define RCTL_CFIEN                      (1 << 19)
#define RCTL_CFI                        (1 << 20)
#define RCTL_DPF                        (1 << 22)
#define RCTL_PMCF                       (1 << 23)
#define RCTL_SECRC                      (1 << 26)
#define RCTL_BSIZE_256                  (3 << 16)
#define RCTL_BSIZE_512                  (2 << 16)
#define RCTL_BSIZE_1024                 (1 << 16)
#define RCTL_BSIZE_2048                 (0 << 16)
#define RCTL_BSIZE_4096                 ((3 << 16) | (1 << 25))
#define RCTL_BSIZE_8192                 ((2 << 16) | (1 << 25))
#define RCTL_BSIZE_16384                ((1 << 16) | (1 << 25))
#define CMD_EOP                         (1 << 0)
#define CMD_IFCS                        (1 << 1)
#define CMD_IC                          (1 << 2)
#define CMD_RS                          (1 << 3)
#define CMD_RPS                         (1 << 4)
#define CMD_VLE                         (1 << 6)
#define CMD_IDE                         (1 << 7)
#define TCTL_EN                         (1 << 1)
#define TCTL_PSP                        (1 << 3)
#define TCTL_CT_SHIFT                   4
#define TCTL_COLD_SHIFT                 12
#define TCTL_SWXOFF                     (1 << 22)
#define TCTL_RTLC                       (1 << 24)
#define TSTA_DD                         (1 << 0)
#define TSTA_EC                         (1 << 1)
#define TSTA_LC                         (1 << 2)

enum class E1000InterruptCause{
    LinkStatusChanged,
    RXMinThresholdReached,
    ReceiverTimerInterrupt,
    UnknownCause
};

void handlePhysicalNetworkInterfaceInterrupt(unsigned int interruptParam, unsigned int eax){
    PhysicalNetworkInterface* pPhysicalNetworkInterface = (PhysicalNetworkInterface*)interruptParam;

    unsigned int status = pPhysicalNetworkInterface->readCommand(INTERRUPT_CAUSE_READ_REGISTER);

    E1000InterruptCause interruptCause = E1000InterruptCause::UnknownCause;
    if(status & 0x04){
        // Link status changed, make sure link is up
        unsigned int ctrlRegister = pPhysicalNetworkInterface->readCommand(DEVICE_CONTROL_REGISTER);
        pPhysicalNetworkInterface->writeCommand(DEVICE_CONTROL_REGISTER, ctrlRegister | 0x40);

        interruptCause = E1000InterruptCause::LinkStatusChanged;
    }
    else if(status & 0x10){
        // Don't know what this does honestly
        interruptCause = E1000InterruptCause::RXMinThresholdReached;
    }
    else if(status & 0x80){
        interruptCause = E1000InterruptCause::ReceiverTimerInterrupt;
    }

    if(interruptCause==E1000InterruptCause::ReceiverTimerInterrupt){
        Callable<Pair<unsigned char*, unsigned int>>* pPacketHandler = pPhysicalNetworkInterface->pPacketHandler;
        Pair<unsigned char*, unsigned int> readBuffer;
        while((readBuffer = pPhysicalNetworkInterface->getReadBuffer()).first != nullptr){
            if(pPacketHandler!=nullptr){
                pPacketHandler->call({readBuffer.first, readBuffer.second});
            }

            pPhysicalNetworkInterface->finishReadBuffer();
        }
    }
}

PhysicalNetworkInterface* PhysicalNetworkInterface::pPhysicalNetworkInterface = nullptr;

PhysicalNetworkInterface* PhysicalNetworkInterface::getPhysicalNetworkInterface(){
    return pPhysicalNetworkInterface;
}

void PhysicalNetworkInterface::initialize(MemoryManager* pMemoryManager){
    unsigned char* physicalNetworkInterfaceAddr = pMemoryManager->allocate(alignof(PhysicalNetworkInterface), sizeof(PhysicalNetworkInterface));
    if(physicalNetworkInterfaceAddr == nullptr){
        Screen* pScreen = Screen::getScreen();
        pScreen->printk((char*)"Failed to allocate memory for PhysicalNetworkInterface!\n");
        while(true);
    }

    pPhysicalNetworkInterface = new(physicalNetworkInterfaceAddr) PhysicalNetworkInterface();
}

void PhysicalNetworkInterface::writeCommand(unsigned short address, unsigned int value){
    if(usingMemMappedRegisters){
        *((volatile unsigned int*)(ioBase+address)) = value;
    }
    else{
        portDwordOut(ioBase, address);
        portDwordOut(ioBase+4, value);
    }
}

unsigned int PhysicalNetworkInterface::readCommand(unsigned short address){
    unsigned int retVal = 0;
    
    if(usingMemMappedRegisters){
        retVal = *((volatile unsigned int*)(ioBase+address));
    }
    else{
        portDwordOut(ioBase, address);
        retVal = portDwordIn(ioBase+4);
    }

    return retVal;
}

unsigned int PhysicalNetworkInterface::readEEPROM(unsigned int addr){
    unsigned short data = 0;
    unsigned int tmp = 0;
    writeCommand(EEPROM_READ_REGISTER, 1 | (addr << 8));
    while(!((tmp = readCommand(EEPROM_READ_REGISTER)) & 0x10));
    data = (unsigned short)((tmp >> 16) & 0xFFFF);
    return data;
}

PhysicalNetworkInterface::PhysicalNetworkInterface()
    :
    ioBase(0),
    interruptsEnabled(false),
    pPacketHandler(nullptr)
{
    Screen* pScreen = Screen::getScreen();
    
    unsigned int ldevice_ID = (unsigned int)INTEL_82540EM_DEVICE_ID;
    unsigned int lvendor_ID = (unsigned int)INTEL_PCI_VENDOR_ID;
    bool found = false;
    unsigned char bus = 0;
    unsigned char slot = 0;
    do{
        for(slot=0; slot<32;slot++){
             if(pciReadDword(bus, slot, 0, 0) == ((ldevice_ID << 16) | lvendor_ID)){
                unsigned int classCode = (pciReadDword(bus, slot, 0, CLASS_CODE_AND_REVISION_OFFSET) >> 8);
                found = (classCode==ETHERNET_ADAPTER_CLASS_CODE);
             }
             if(found){
                break;
             }
        }
        if(found){
            break;
        }
    } while((++bus) != 0);
    if(found){
        pScreen->printk((char*)"Found the 82540EM ethernet controller, bus: ");
        pScreen->printk(bus);
        pScreen->printk((char*)" slot: ");
        pScreen->printk(slot);
        pScreen->printk((char*)"\n");
    }
    else{
        pScreen->printk((char*)"Didn't find an 82540EM ethernet controller, OS won't start without it.\n");
        while(1);
    }

    unsigned int statusAndCommandRegister = pciReadDword(bus, slot, 0, STATUS_AND_COMMAND_REGISTER_OFFSET);
    bool shouldAttempUsingMemRegisters = false;
    if((statusAndCommandRegister & 0x2)!=0) shouldAttempUsingMemRegisters=true;
    if((statusAndCommandRegister & 0b100)==0){
        pScreen->printk((char*)"Bus mastering is disabled...\n");
        pciWriteDword(bus, slot, 0, 0x4, statusAndCommandRegister | 0b100);
        statusAndCommandRegister = pciReadDword(bus, slot, 0, STATUS_AND_COMMAND_REGISTER_OFFSET);
        if((statusAndCommandRegister & 0b100)==0){
            pScreen->printk((char*)"Trying to enable bus mastering doesn't work, OS won't start without it.\n");
            while(1);
        }
        else{
            pScreen->printk((char*)"Managed to enable bus mastering.\n");
        }
    }
    else{
        pScreen->printk((char*)"Bus mastering is enabled...\n");
    }

    unsigned int allZeros = pciReadDword(bus, slot, 0, RESERVED_BAR_OFFSET);
    if(allZeros!=0){
        pScreen->printk((char*)"82540EM uses 64-bit BARs which OS does not support, OS won't start.\n");
        while(1);
    }

    if(shouldAttempUsingMemRegisters){
        unsigned int ioBar0 = pciReadDword(bus, slot, 0, MEM_BAR_OFFSET);
        if((ioBar0 & 0x1)!=0 || (ioBar0 & 0x6)!=0 || (ioBar0 & 0xFFFFFFF0)==0){
            unsigned int ioBar2 = pciReadDword(bus, slot, 0, IO_BAR_OFFSET);
            if((ioBar2 & 0x1)==0 || (ioBar2 & 0x6)!=0 || (ioBar2 & 0xFFFFFFF8)==0){
                pScreen->printk((char*)"Even though memory mapped IO should be possible, both IO BARs are unusable...\n");
                while(1);
            }
            else{
                ioBase = ioBar2 & 0xFFFFFFF8;
                usingMemMappedRegisters = false;
            }
        }
        else{
            ioBase = ioBar0 & 0xFFFFFFF0;
            usingMemMappedRegisters = true;
        }
    }
    else{
        unsigned int ioBar2 = pciReadDword(bus, slot, 0, IO_BAR_OFFSET);
        if((ioBar2 & 0x1)==0 || (ioBar2 & 0x6)!=0 || (ioBar2 & 0xFFFFFFF8)==0){
            pScreen->printk((char*)"Memory mapped IO should not be possible, but BAR2 also seems unusable...\n");
            while(1);
        }
        else{
            ioBase = ioBar2 & 0xFFFFFFF8;
            usingMemMappedRegisters = false;
        }
    }
    
    bool eepromExists = false;
    writeCommand(EEPROM_READ_REGISTER, 0x1);
    unsigned int eepromBar = readCommand(EEPROM_READ_REGISTER);
    pScreen->printk((char*)"EEPROM BAR: ");
    pScreen->printk(eepromBar);
    pScreen->printk((char*)"\n");
    if((eepromBar & 0x10)!=0){
        pScreen->printk((char*)"EEPROM bar was found.\n");
    }
    else{
        //TODO: eeprom support is not strictly necessary
        // need to find an emulator which doesn't use an eeprom and then rewrite things to add support for this
        pScreen->printk((char*)"EEPROM bar was not found so OS won't start.\n");
        while(1);
    }

    #if E2E_TESTING
    SerialLog* pSerialLog = SerialLog::getSerialLog();
    pSerialLog->log((char*)"NCM: MAC: ");
    #endif

    pScreen->printk((char*)"MAC-address: ");
    for(int i=0; i<3; i++){
        unsigned int eepromRetValue = readEEPROM(i);
        mac[2*i] = eepromRetValue & 0xFF;
        mac[2*i+1] = eepromRetValue >> 8;

        #if E2E_TESTING
        if(((unsigned int)mac[2*i])<0x10){
            pSerialLog->log((char*)"0");
        }
        pSerialLog->logHex((unsigned int)mac[2*i]);
        pSerialLog->log((char*)":");
        if(((unsigned int)mac[2*i+1])<0x10){
            pSerialLog->log((char*)"0");
        }
        pSerialLog->logHex((unsigned int)mac[2*i+1]);
        if(i!=2){
            pSerialLog->log((char*)":");
        }
        #endif

        pScreen->printkHex((unsigned int)mac[2*i]);
        pScreen->printk((char*)"-");
        pScreen->printkHex((unsigned int)mac[2*i+1]);
        if(i!=2) pScreen->printk((char*)"-");
    }
    pScreen->printk((char*)"\n");

    #if E2E_TESTING
    pSerialLog->log((char*)"\n");
    #endif

    unsigned int ctrlRegister = readCommand(DEVICE_CONTROL_REGISTER);
    writeCommand(DEVICE_CONTROL_REGISTER, ctrlRegister | 0x40);
    for(int i = 0; i < 0x80; i++) writeCommand(MULTICAST_TABLE_ARRAY_FIRST_REGISTER + i*4, 0);
    pScreen->printk((char*)"Linkup is done.\n");

    interruptLine = pciReadDword(bus, slot, 0, MAX_LAT_MIN_GRANT_INT_PIN_INT_LINE_OFFSET) & 0xFF;
    pScreen->printk((char*)"Interrupt line is: ");
    pScreen->printk(interruptLine);
    pScreen->printk((char*)"\n");
    writeCommand(INTERRUPT_MASK_SR_REGISTER, 0xFFFF);
    writeCommand(INTERRUPT_MASK_C_REGISTER, 0xFFFF);
    readCommand(INTERRUPT_CAUSE_READ_REGISTER);
    pScreen->printk((char*)"Interrupts have been disabled\n");

    rxInit();
    txInit();

    ((e1000TxDescTCPIPContext*)txDescs)[currentTx].ipcss = 14;
    ((e1000TxDescTCPIPContext*)txDescs)[currentTx].ipcso = 14+10;
    ((e1000TxDescTCPIPContext*)txDescs)[currentTx].ipcse = 14+19;
    ((e1000TxDescTCPIPContext*)txDescs)[currentTx].tucss = 0;
    ((e1000TxDescTCPIPContext*)txDescs)[currentTx].tucso = 0;
    ((e1000TxDescTCPIPContext*)txDescs)[currentTx].tucse = 0;
    ((e1000TxDescTCPIPContext*)txDescs)[currentTx].paylenLow = 0;
    ((e1000TxDescTCPIPContext*)txDescs)[currentTx].paylenHighAndDtyp = 0;
    ((e1000TxDescTCPIPContext*)txDescs)[currentTx].tucmd = (0 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (0 << 7);
    ((e1000TxDescTCPIPContext*)txDescs)[currentTx].staAndRsv = 0;
    ((e1000TxDescTCPIPContext*)txDescs)[currentTx].hdrlen = 0;
    ((e1000TxDescTCPIPContext*)txDescs)[currentTx].mss = 0;
    writeCommand(0x3818, currentTx);
    currentTx = (currentTx + 1) % NUM_TX_DESCRIPTORS;
    return;
}

// Receive and transmit descriptors are explained at
// https://www.intel.com/content/dam/doc/manual/pci-pci-x-family-gbe-controllers-software-dev-manual.pdf
// page 19 -> ...
void PhysicalNetworkInterface::rxInit(){
    for(int i = 0; i < NUM_RX_DESCRIPTORS; i++){
        rxDescs[i].addrLow = (unsigned int)((unsigned int)rxBufferSpace+RX_BUFFER_SIZE*i);
        rxDescs[i].addrHigh = 0;
        rxDescs[i].status = 0;
    }
    writeCommand(0x2800, (unsigned int)rxDescs);
    writeCommand(0x2804, 0);
    writeCommand(0x2808, 32*16);
    writeCommand(0x2810, 0);
    writeCommand(0x2818, 32-1);
    writeCommand(0x0100, RCTL_EN| RCTL_SBP| RCTL_UPE | RCTL_MPE | RCTL_LBM_NONE | RTCL_RDMTS_HALF | RCTL_BAM | RCTL_SECRC  | RCTL_BSIZE_2048);
}

// Receive and transmit descriptors are explained at
// https://www.intel.com/content/dam/doc/manual/pci-pci-x-family-gbe-controllers-software-dev-manual.pdf
// page 19 -> ...
void PhysicalNetworkInterface::txInit(){
    for(int i = 0; i < NUM_TX_DESCRIPTORS; i++){
        txDescs[i].status = TSTA_DD;
    }
    writeCommand(0x3800, (unsigned int)txDescs);
    writeCommand(0x3804, 0);
    //now setup total length of descriptors
    writeCommand(0x3808, 8*16);
    //setup numbers
    writeCommand(0x3810, 0);
    writeCommand(0x3818, 0);
    writeCommand(0x0400,  TCTL_EN
        | TCTL_PSP
        | (15 << TCTL_CT_SHIFT)
        | (64 << TCTL_COLD_SHIFT)
        | TCTL_RTLC);
    //comments from original author (https://wiki.osdev.org/Intel_Ethernet_i217):
    //(I haven't tested yet whether this is necessary myself)
    // This line of code overrides the one before it but I left both to highlight that the previous one works with e1000 cards, but for the e1000e cards 
    // you should set the TCTRL register as follows. For detailed description of each bit, please refer to the Intel Manual.
    // In the case of I217 and 82577LM packets will not be sent if the TCTRL is not configured using the following bits.
    writeCommand(0x0400,  0b0110000000000111111000011111010);
    writeCommand(0x0410,  0x0060200A);
}

unsigned char* PhysicalNetworkInterface::getWriteBuffer(){
    // if txDesc[currentTx].status==0, this means txDesc[currentTx] points to a packet which has not been send yet
    if(txDescs[currentTx].status==0){
        return nullptr;
    }
    
    return txBufferSpace+TX_BUFFER_SIZE*currentTx;
}

void PhysicalNetworkInterface::finishWriteBuffer(unsigned int length, bool isIpv4Packet){
    if(length < ETHERNET_SIMPLE_HEADER_SIZE){
        return;
    }

    if(length > ETHERNET_MTU+ETHERNET_SIMPLE_HEADER_SIZE){
        return;
    }

    if(txDescs[currentTx].status==0){
        return;
    }

    if(!isIpv4Packet){
        ((e1000TxDescLegacy*)txDescs)[currentTx].addrLow = (unsigned int)((unsigned int)txBufferSpace+TX_BUFFER_SIZE*currentTx);
        ((e1000TxDescLegacy*)txDescs)[currentTx].addrHigh = 0;

        ((e1000TxDescLegacy*)txDescs)[currentTx].length = length;
        ((e1000TxDescLegacy*)txDescs)[currentTx].cso = 0;
        ((e1000TxDescLegacy*)txDescs)[currentTx].cmd = CMD_EOP | CMD_IFCS | CMD_RS;
        ((e1000TxDescLegacy*)txDescs)[currentTx].status = 0;
        ((e1000TxDescLegacy*)txDescs)[currentTx].css = 0;
        ((e1000TxDescLegacy*)txDescs)[currentTx].special = 0;
    }
    else{
        if(length < IPV4_MINIMAL_HEADER_SIZE+ETHERNET_SIMPLE_HEADER_SIZE){
            return;
        }

        ((e1000TxDescTCPIPData*)txDescs)[currentTx].addrLow = (unsigned int)((unsigned int)txBufferSpace+TX_BUFFER_SIZE*currentTx);
        ((e1000TxDescTCPIPData*)txDescs)[currentTx].addrHigh = 0;

        ((e1000TxDescTCPIPData*)txDescs)[currentTx].dtalenLow = length;
        ((e1000TxDescTCPIPData*)txDescs)[currentTx].dtalenHighAndDtype = (1 << 4);
        ((e1000TxDescTCPIPData*)txDescs)[currentTx].dcmd = (1 << 0) | (0 << 1) | (1 << 3) | (1 << 5) | (0 << 7);
        ((e1000TxDescTCPIPData*)txDescs)[currentTx].staAndRsv = 0;
        ((e1000TxDescTCPIPData*)txDescs)[currentTx].popts = (1 << 0) | (0 << 1);
        ((e1000TxDescTCPIPData*)txDescs)[currentTx].special = 0;
    }

    currentTx = (currentTx + 1) % NUM_TX_DESCRIPTORS;

    // Indicate the new head of the tx descriptors
    writeCommand(0x3818, currentTx);
}

Pair<unsigned char*, unsigned int> PhysicalNetworkInterface::getReadBuffer(){
    // if (rxDesc[currentTx].status & 0x1)==0, this means rxDesc[currentTx] does not point to a received packet yet
    if((rxDescs[currentRx].status & 0x1)==0){
        return {nullptr, 0};
    }

    return {rxBufferSpace+RX_BUFFER_SIZE*currentRx, (unsigned int)rxDescs[currentRx].length};
}

void PhysicalNetworkInterface::finishReadBuffer(){
    if((rxDescs[currentRx].status & 0x1)==0){
        return;
    }

    rxDescs[currentRx].status = 0;
    unsigned int oldRx = currentRx;
    currentRx = (currentRx+1) % NUM_RX_DESCRIPTORS;

    // Indicate the new head of the rx descriptors
    writeCommand(0x2818, oldRx);
}

void PhysicalNetworkInterface::registerPacketHandler(Callable<Pair<unsigned char*, unsigned int>>* pNewPacketHandler){
    atomicStore((unsigned int*)&pPacketHandler, (unsigned int)pNewPacketHandler);

    if(!interruptsEnabled){
        // Network card interrupts will be enabled on cpu core 0
        CpuCore* pCpuCore = CpuCore::getCpuCore(0);
        if(pCpuCore==nullptr){
            Screen* pScreen = Screen::getScreen();
            pScreen->printk((char*)"Failed to get cpu core 0 to enable network card interrupts\n");
            while(true);
        }

        InterruptType interruptType;
        switch(interruptLine){
            case 9:
                interruptType = InterruptType::Free1;
                break;
            case 10:
                interruptType = InterruptType::Free2;
                break;
            case 11:
                interruptType = InterruptType::Free3;
                break;
            default:
                {
                    Screen* pScreen = Screen::getScreen();
                    pScreen->printk((char*)"Unknown interrupt line for network card\n");
                    while(true);
                }
                break;
        }

        pCpuCore->getInterruptHandlerManager()->setInterruptHandlerParam(interruptType, (unsigned int)this);
        pCpuCore->getInterruptHandlerManager()->setInterruptHandler(interruptType, handlePhysicalNetworkInterfaceInterrupt);

        class NetworkCardEnableInterrupts : public Runnable{
            private:
                PhysicalNetworkInterface* pPhysicalNetworkInterface;

            public:
                NetworkCardEnableInterrupts(PhysicalNetworkInterface* pPhysicalNetworkInterface)
                    :
                    pPhysicalNetworkInterface(pPhysicalNetworkInterface)
                {}

                void run() override{
                    pPhysicalNetworkInterface->writeCommand(INTERRUPT_MASK_SR_REGISTER, 0x04 | 0x10 | 0x80);
                    pPhysicalNetworkInterface->readCommand(INTERRUPT_CAUSE_READ_REGISTER);
                }
        };
        
        NetworkCardEnableInterrupts networkCardEnableInterrupts(this);
        pCpuCore->getInterruptHandlerManager()->withInterruptsDisabled(networkCardEnableInterrupts);
        interruptsEnabled = true;
    }
}

unsigned char* PhysicalNetworkInterface::getMac(){
    return mac;
}

bool PhysicalNetworkInterface::usesMemMappedRegisters(){
    return usingMemMappedRegisters;
}

unsigned int PhysicalNetworkInterface::getIOBase(){
    return ioBase;
}