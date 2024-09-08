#pragma once

#include "../paging/paging.h"

class CurrentPagingStructure{
        friend class CpuCore;

    private:
        CurrentPagingStructure(unsigned int firstPageDirectoryAddr);

        unsigned int firstPageDirectoryAddr;

    public:
        void bind();
        
        unsigned int swapPageDirectory(unsigned int newPageDirectoryPhysicalAddr);

        unsigned int getPageDirectoryPhysicalAddr();
        PDE getPDE(unsigned int pdeIndex);
        PTE getPTE(unsigned int pdeIndex, unsigned int pteIndex);

        void tlbInvalidatePage(Page* pPage);
        void tlbInvalidateAll();
};