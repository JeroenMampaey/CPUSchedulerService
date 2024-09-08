#pragma once

#include "page_allocator.h"

#define LAST_ACCESSIBLE_ADDRESS (((unsigned int)(1024*1023))*((unsigned int)4096) - 1)

class PagingStructures : public PagingStructureAccessor{
    private:
        unsigned char pageDirectory[8192] __attribute__((aligned(4096)));
        Page pageTables[1023];

    public:
        PagingStructures();

        PageDirectory* getPageDirectory() override;
        PageTable* getPageTable(unsigned int i) override;
};