#pragma once

#include "../../cpp_lib/memory_manager.h"

typedef struct BIOSMapEntry{
    unsigned long long regionAddr;
    unsigned long long regionLength;
    unsigned int regionType;
    unsigned int extendedAttributes;
} BIOSMapEntry;

class BIOSMap{
    private:
        static BIOSMap* pBIOSMap;

        BIOSMap();
    
    public:
        static BIOSMap* getBIOSMap();
        static void initialize(MemoryManager* pMemoryManager);

        unsigned int getBIOSMapSize();
        BIOSMapEntry getBIOSMapEntry(unsigned int index);

        void printBIOSMemoryLayout();
        #if E2E_TESTING
        void logBIOSMemoryLayout();
        #endif
};