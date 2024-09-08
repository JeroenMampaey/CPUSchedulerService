#pragma once

#include "paging.h"

class PagingStructureAccessor{
    public:
        virtual PageDirectory* getPageDirectory() = 0;
        virtual PageTable* getPageTable(unsigned int i) = 0;
};

class PageAllocator{
    private:
        bool checkPageTableFullyAllocated(PageTable* pPageTable);
        bool checkPageTableFullyNotAllocated(PageTable* pPageTable);

        unsigned int numFreeAtBegin(PageTable* pPageTable);
        unsigned int numFreeAtEnd(PageTable* pPageTable);
        unsigned int findMaxFreeSpaceStartingFromTable(unsigned int startingPageTableIndex);

        void allocatePagesStartingAtIndex(unsigned int startingPageIndex, unsigned int numPages);

        PagingStructureAccessor* pAccessor;

    public:
        PageAllocator(PagingStructureAccessor* pAccessor);

        void allocatePageRange(unsigned int firstPageIndex, unsigned int lastPageIndex);
        unsigned int allocateContiguousPages(unsigned int numPages);
        void freeContiguousPages(unsigned int virtualMemoryPointer);

        #if E2E_TESTING
        void logState();
        #endif
};