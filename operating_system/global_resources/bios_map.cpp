#include "bios_map.h"

#include "../cpp/placement_new.h"
#include "serial_log.h"
#include "screen.h"

#define BIOS_MAP_ADDR ((unsigned char*)0x800)

BIOSMap* BIOSMap::pBIOSMap = nullptr;

BIOSMap* BIOSMap::getBIOSMap(){
    return pBIOSMap;
}

void BIOSMap::initialize(MemoryManager* pMemoryManager){
    unsigned char* biosMapAddr = pMemoryManager->allocate(alignof(BIOSMap), sizeof(BIOSMap));
    if(biosMapAddr == nullptr){
        Screen* pScreen = Screen::getScreen();
        pScreen->printk((char*)"Failed to allocate memory for BIOS map!\n");
        while(true);
    }

    pBIOSMap = new(biosMapAddr) BIOSMap();
}

BIOSMap::BIOSMap(){}

unsigned int BIOSMap::getBIOSMapSize(){
    return  *((unsigned int*)BIOS_MAP_ADDR);
}

BIOSMapEntry BIOSMap::getBIOSMapEntry(unsigned int index){
    if(index>=getBIOSMapSize()) return {};

    unsigned char* mapEntryAddr = BIOS_MAP_ADDR + 4 + 3*8*index;

    BIOSMapEntry biosMapEntry;

    biosMapEntry.regionAddr = *((unsigned long long*)mapEntryAddr);
    biosMapEntry.regionLength = *((unsigned long long*)(mapEntryAddr+8));
    biosMapEntry.regionType = *((unsigned int*)(mapEntryAddr+16));
    biosMapEntry.extendedAttributes = *((unsigned int*)(mapEntryAddr+20));

    return biosMapEntry;
}

void BIOSMap::printBIOSMemoryLayout(){
    Screen* pScreen = Screen::getScreen();
    unsigned int biosMapSize = getBIOSMapSize();
    pScreen->printk((char*)"BIOS map size: ");
    pScreen->printk(biosMapSize);
    pScreen->printk((char*)"\n");
    pScreen->printk((char*)"BIOS map:\n");

    for(int i=0; i<biosMapSize; i++){
        BIOSMapEntry biosMapEntry = getBIOSMapEntry(i);
        pScreen->printk((char*)"        0x");
        pScreen->printkHex(biosMapEntry.regionAddr);
        pScreen->printk((char*)"|");
        pScreen->printk((char*)"0x");
        pScreen->printkHex(biosMapEntry.regionLength);
        pScreen->printk((char*)"|");
        pScreen->printk(biosMapEntry.regionType);
        pScreen->printk((char*)"|");
        pScreen->printk(biosMapEntry.extendedAttributes);
        pScreen->printk((char*)"\n");
    }
}

#if E2E_TESTING
void BIOSMap::logBIOSMemoryLayout(){
    SerialLog* pSerialLog = SerialLog::getSerialLog();
    pSerialLog->log((char*)"BM: log begin\n");

    unsigned int biosMapSize = getBIOSMapSize();
    for(int i=0; i<biosMapSize; i++){
        BIOSMapEntry biosMapEntry = getBIOSMapEntry(i);
        pSerialLog->log((char*)"BM: biosmapentry 0x");
        pSerialLog->logHex(biosMapEntry.regionAddr);
        pSerialLog->log((char*)" 0x");
        pSerialLog->logHex(biosMapEntry.regionLength);
        pSerialLog->log((char*)" ");
        pSerialLog->log(biosMapEntry.regionType);
        pSerialLog->log((char*)" ");
        pSerialLog->log(biosMapEntry.extendedAttributes);
        pSerialLog->log((char*)"\n");
    }

    pSerialLog->log((char*)"BM: log end\n");
}
#endif