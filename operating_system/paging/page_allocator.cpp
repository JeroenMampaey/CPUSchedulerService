#include "page_allocator.h"

// Bit 9-10 of PDEs and PTEs are used to keep track if the pages are allocated
// PDE bits 9-10:
//  -if 0, the pages managed by the page table pointed to by the PDE are not all allocated
//  -if 1, some of the pages managed by the page table pointed to by the PDE are allocated
//  -if 2, all the pages managed by the page table pointed to by the PDE are allocated
// PTE bits 9-10:
//  -if 0, the page pointed to by the PTE is free
//  -if not 0, the page pointed to by the PTE is allocated, the exact value is used to keep track which pages should be
//   freed when freeContiguousPages(...) is called (basically if allocateContiguousPages is called twice and allocates
//   two neighbouring regions, these regions will have a different value for bits 9-10)

#define PAGE_TABLE_FULLY_NOT_ALLOCATED_CODE 0
#define PAGE_TABLE_PARTLY_ALLOCATED_CODE 1
#define PAGE_TABLE_FULLY_ALLOCATED_CODE 2

#define LAST_POSSIBLE_PDE_INDEX 1023

PageAllocator::PageAllocator(PagingStructureAccessor* pAccessor)
    :
    pAccessor(pAccessor)
{
    PageDirectory* pPageDirectory = pAccessor->getPageDirectory();

    for(int i=0; i<1023; i++){
        PageTable* pPageTable = pAccessor->getPageTable(i);

        for(int j=0; j<1024; j++){
            pPageTable->pageTableEntries[j] &= ~(unsigned int)(0x3 << 9);
        }

        pPageDirectory->pageDirectoryEntries[i] &= ~(unsigned int)(0x3 << 9);
    }
}

void PageAllocator::allocatePageRange(unsigned int firstPageIndex, unsigned int lastPageIndex){
    PageDirectory* pPageDirectory = pAccessor->getPageDirectory();

    unsigned int firstPDEIndex = firstPageIndex/1024;
    unsigned int firstPTEIndex = (firstPageIndex % 1024);
    unsigned int lastPDEIndex = lastPageIndex/1024;
    unsigned int lastPTEIndex = (lastPageIndex % 1024);

    for(int i=firstPDEIndex; i<=lastPDEIndex && i<=LAST_POSSIBLE_PDE_INDEX; i++){
        int pteBegin = 0;
        int pteEnd = 1023;

        if(i==firstPDEIndex){
            pteBegin = firstPTEIndex;
        }
        
        if(i==lastPDEIndex){
            pteEnd = lastPTEIndex;
        }

        PageTable* pPageTable = pAccessor->getPageTable(i);
        for(int j=pteBegin; j<=pteEnd; j++){
            pPageTable->pageTableEntries[j] |= (1 << 9);
        }

        if(checkPageTableFullyAllocated(pPageTable)){
            pPageDirectory->pageDirectoryEntries[i] &= ~(unsigned int)(0x3 << 9);
            pPageDirectory->pageDirectoryEntries[i] |= (PAGE_TABLE_FULLY_ALLOCATED_CODE << 9);
        }
        else{
            pPageDirectory->pageDirectoryEntries[i] &= ~(unsigned int)(0x3 << 9);
            pPageDirectory->pageDirectoryEntries[i] |= (PAGE_TABLE_PARTLY_ALLOCATED_CODE << 9);
        }
    }
}

bool PageAllocator::checkPageTableFullyAllocated(PageTable* pPageTable){
    for(int i=0; i<1024; i++){
        if((pPageTable->pageTableEntries[i] & (0x3 << 9)) == 0) return false;
    }

    return true;
}

bool PageAllocator::checkPageTableFullyNotAllocated(PageTable* pPageTable){
    for(int i=0; i<1024; i++){
        if((pPageTable->pageTableEntries[i] & (0x3 << 9)) != 0) return false;
    }

    return true;
}

unsigned int PageAllocator::numFreeAtBegin(PageTable* pPageTable){
    if(pPageTable==nullptr) return 0;

    unsigned int retVal = 0;
    for(int i=0; i<1024; i++){
        if((pPageTable->pageTableEntries[i] & (0x3 << 9))==0){
            retVal++;
        }
        else{
            return retVal;
        }
    }

    return retVal;
}

unsigned int PageAllocator::numFreeAtEnd(PageTable* pPageTable){
    if(pPageTable==nullptr) return 0;

    unsigned int retVal = 0;

    for(int i=1023; i>=0; i--){
        if((pPageTable->pageTableEntries[i] & (0x3 << 9))==0){
            retVal++;
        }
        else{
            return retVal;
        }
    }

    return retVal;
}

unsigned int PageAllocator::findMaxFreeSpaceStartingFromTable(unsigned int startingPageTableIndex){
    if(startingPageTableIndex>=1023) return 0;

    unsigned int retVal = 0;
    unsigned int currentCounter = 0;

    PageTable* pStartingPageTable = pAccessor->getPageTable(startingPageTableIndex);
    for(int i=0; i<1024; i++){
        if((pStartingPageTable->pageTableEntries[i] & (0x3 << 9))==0){
            currentCounter += 1;
        }
        else{
            retVal = (currentCounter > retVal) ? currentCounter : retVal;
            currentCounter = 0;
        }
    }

    if(currentCounter==0){
        return retVal;
    }

    unsigned int numFreeAtEnd = numFreeAtBegin(pAccessor->getPageTable(startingPageTableIndex+1));
    currentCounter += numFreeAtEnd;
    if(numFreeAtEnd==1024){
        currentCounter += numFreeAtBegin(pAccessor->getPageTable(startingPageTableIndex+2));
    }

    retVal = (currentCounter > retVal) ? currentCounter : retVal;

    return retVal;
}

unsigned int PageAllocator::allocateContiguousPages(unsigned int numPages){
    if(numPages==0) return ((unsigned int)LAST_POSSIBLE_PDE_INDEX*1024)*((unsigned int)0x1000);

    // ...
    // 1024+1022 -> 0
    // 1024+1023 -> 1
    // 1024+1024 -> 1
    // 1024+1024+1 -> 1
    // ...
    // 1024+1024+1022 -> 1
    // 1024+1024+1023 -> 2
    // 1024+1024+1023 -> 2
    // 1024+1024+1024 -> 2
    // ... -> 2
    // 1024+1024+1024+1022 -> 2
    // 1024+1024+1024+1023 -> 3
    // ...
    unsigned int MINNUMALLOCATEDPAGETABLES = (numPages < 2*1023) ?
        0
        :
        (numPages-1023)/1024;
    
    unsigned int MINNUMOTHERFREEPAGES = numPages-MINNUMALLOCATEDPAGETABLES*1024;

    PageDirectory* pPageDirectory = pAccessor->getPageDirectory();

    unsigned int i=0;
    while(i<1023){
        if(((pPageDirectory->pageDirectoryEntries[i] >> 9) & 0x3)!=PAGE_TABLE_FULLY_ALLOCATED_CODE){
            unsigned int currentPageTableCode = (pPageDirectory->pageDirectoryEntries[i] >> 9) & 0x3;
            
            unsigned int remainingRequiredUnallocatedPageTables = MINNUMALLOCATEDPAGETABLES;
            if(currentPageTableCode==0 && remainingRequiredUnallocatedPageTables!=0){
                remainingRequiredUnallocatedPageTables--;
            }

            unsigned int j=0;
            for(; j<remainingRequiredUnallocatedPageTables; j++){
                if((pPageDirectory->pageDirectoryEntries[i+j+1] & (0x3 << 9))!=0){
                    break;
                }
            }

            if(j!=remainingRequiredUnallocatedPageTables){
                // Not enough of the next page tables are not full
                // (pPageDirectory->pageDirectoryEntries[i+j+1] & (0x3 << 0))!=0
                i = (i+j+1);
            }
            else if(MINNUMALLOCATEDPAGETABLES==0){
                unsigned int foundNumOtherFreePages = findMaxFreeSpaceStartingFromTable(i);
                if(foundNumOtherFreePages>=MINNUMOTHERFREEPAGES){
                    break;
                }
                else{
                    i++;
                }
            }
            else{
                unsigned int foundNumOtherFreePages = 0;

                if(currentPageTableCode!=0){
                    foundNumOtherFreePages += numFreeAtEnd(pAccessor->getPageTable(i));
                }
                
                unsigned int numFreeAtEnd = numFreeAtBegin(pAccessor->getPageTable((i+j+1)));
                foundNumOtherFreePages += numFreeAtEnd;
                if(numFreeAtEnd==1024){
                    foundNumOtherFreePages += numFreeAtBegin(pAccessor->getPageTable((i+j+2)));
                }

                if(foundNumOtherFreePages>=MINNUMOTHERFREEPAGES){
                    break;
                }
                else{
                    i = (i+j+1);
                }
            }
        }
        else{
            i++;
        }
    }

    if(i>=1023){
        return ((unsigned int)LAST_POSSIBLE_PDE_INDEX*1024)*((unsigned int)0x1000);
    }

    PageTable* pPageTable = pAccessor->getPageTable(i);
    unsigned int foundPageTableIndex = 0;
    if(MINNUMALLOCATEDPAGETABLES==0){
        unsigned int lastjValue = 0;
        unsigned int currentCounter = 0;
        for(int j=0; j<1024; j++){
            if((pPageTable->pageTableEntries[j] & (0x3 << 9))==0){
                currentCounter += 1;
                if(currentCounter >= MINNUMOTHERFREEPAGES){
                    break;
                }
            }
            else{
                lastjValue = j+1;
                currentCounter = 0;
            }
        }
        foundPageTableIndex = i*1024 + lastjValue;
    }
    else{
        int j = 1023;
        for(; j>=0; j--){
            if((pPageTable->pageTableEntries[j] & (0x3 << 9))!=0){
                break;
            }
        }
        // (pPageTable->pageTableEntries[j] & (0x3 << 9))!=0
        j++;
        foundPageTableIndex = i*1024 + j;
    }

    /*Screen* pScreen = getScreen();
    pScreen->printk((char*)"Found page index: ");
    pScreen->printk(foundPageTableIndex);
    pScreen->printk((char*)" ");
    pScreen->printk(numPages);
    while(1);*/

    allocatePagesStartingAtIndex(foundPageTableIndex, numPages);

    return foundPageTableIndex*0x1000;
}

void PageAllocator::allocatePagesStartingAtIndex(unsigned int startingPageIndex, unsigned int numPages){
    if(numPages==0) return;

    PageDirectory* pPageDirectory = pAccessor->getPageDirectory();

    unsigned int endingPageIndex = startingPageIndex+numPages-1;

    unsigned int preFirstPageCode = 1;
    unsigned int postLastPageCode = 1;
    if(startingPageIndex>0){
        unsigned int pdeIndex = (startingPageIndex-1)/1024;
        unsigned int pteIndex = ((startingPageIndex-1) % 1024);
        PageTable* pPageTable = pAccessor->getPageTable(pdeIndex);
        preFirstPageCode = (pPageTable->pageTableEntries[pteIndex] >> 9) & 0x3;
    }
    if(endingPageIndex<1023*1024-1){
        unsigned int pdeIndex = (endingPageIndex+1)/1024;
        unsigned int pteIndex = ((endingPageIndex+1) % 1024);
        PageTable* pPageTable = pAccessor->getPageTable(pdeIndex);
        postLastPageCode = (pPageTable->pageTableEntries[pteIndex] >> 9) & 0x3;
    }

    unsigned int newPageCode = 0;
    if(preFirstPageCode==postLastPageCode){
        newPageCode = (preFirstPageCode==1) ? 2 : 1;
    }
    else if(preFirstPageCode==0){
        newPageCode = (postLastPageCode==1) ? 2 : 1;
    }
    else if(postLastPageCode==0){
        newPageCode = (preFirstPageCode==1) ? 2 : 1;
    }
    else{
        // preFirstPageCode!=postLastPageCode
        // preFirstPageCode!=0
        // postLastPageCode!=0
        newPageCode = (1+2+3)-(preFirstPageCode+postLastPageCode);
    }

    unsigned int firstPDEIndex = startingPageIndex/1024;
    unsigned int firstPTEIndex = (startingPageIndex % 1024);
    unsigned int lastPDEIndex = endingPageIndex/1024;
    unsigned int lastPTEIndex = (endingPageIndex % 1024);

    /*Screen* pScreen = getScreen();
    pScreen->printk((char*)"Found page index: ");
    pScreen->printk(firstPDEIndex);
    pScreen->printk((char*)" ");
    pScreen->printk(firstPTEIndex);
    pScreen->printk((char*)" ");
    pScreen->printk(lastPDEIndex);
    pScreen->printk((char*)" ");
    pScreen->printk(lastPTEIndex);
    pScreen->printk((char*)"\n");
    while(1);*/

    for(int i=firstPDEIndex; i<=lastPDEIndex; i++){
        int pteBegin = 0;
        int pteEnd = 1023;

        if(i==firstPDEIndex){
            pteBegin = firstPTEIndex;
        }
        
        if(i==lastPDEIndex){
            pteEnd = lastPTEIndex;
        }

        PageTable* pPageTable = pAccessor->getPageTable(i);
        for(int j=pteBegin; j<=pteEnd; j++){
            pPageTable->pageTableEntries[j] |= (newPageCode << 9);
        }

        /*Screen* pScreen = getScreen();
        pScreen->printk((char*)"Found pte range: ");
        pScreen->printk(pteBegin);
        pScreen->printk((char*)" ");
        pScreen->printk(pteEnd);
        pScreen->printk((char*)"\n");
        while(1);*/

        if(pteBegin==0 && pteEnd==1023){
            pPageDirectory->pageDirectoryEntries[i] &= ~(unsigned int)(0x3 << 9);
            pPageDirectory->pageDirectoryEntries[i] |= (PAGE_TABLE_FULLY_ALLOCATED_CODE << 9);
        }
        else if(checkPageTableFullyAllocated(pPageTable)){
            pPageDirectory->pageDirectoryEntries[i] &= ~(unsigned int)(0x3 << 9);
            pPageDirectory->pageDirectoryEntries[i] |= (PAGE_TABLE_FULLY_ALLOCATED_CODE << 9);
        }
        else{
            pPageDirectory->pageDirectoryEntries[i] &= ~(unsigned int)(0x3 << 9);
            pPageDirectory->pageDirectoryEntries[i] |= (PAGE_TABLE_PARTLY_ALLOCATED_CODE << 9);
        }
    }
}

void PageAllocator::freeContiguousPages(unsigned int virtualMemoryPointer){
    unsigned int virtualPageIndex = virtualMemoryPointer/4096;

    unsigned int firstPDEIndex = virtualPageIndex/1024;
    unsigned int firstPTEIndex = (virtualPageIndex % 1024);

    if(firstPDEIndex >= 1023) return;

    /*pScreen->printk(pteBegin);
    pScreen->printk((char*)" ");
    pScreen->printk(j);
    pScreen->printk((char*)"\n");
    while(1);*/

    PageDirectory* pPageDirectory = pAccessor->getPageDirectory();
    
    unsigned int pageCode = (pAccessor->getPageTable(firstPDEIndex)->pageTableEntries[firstPTEIndex] >> 9) & 0x3;

    for(int i=firstPDEIndex; i<1023; i++){
        PageTable* pPageTable = pAccessor->getPageTable(i);

        unsigned int pteBegin = (i==firstPDEIndex) ? firstPTEIndex : 0;
        unsigned int j = pteBegin;
        for(; j<1024; j++){
            if(((pPageTable->pageTableEntries[j] >> 9) & 0x3)!=pageCode){
                break;
            }

            pPageTable->pageTableEntries[j] &= ~(unsigned int)(0x3 << 9);
        }

        if(pteBegin==0 && j==1024){
            pPageDirectory->pageDirectoryEntries[i] &= ~(unsigned int)(0x3 << 9);
        }
        else if(pteBegin==j){
            // Don't do anything because for-loop immediately performed break
        }
        else if(checkPageTableFullyNotAllocated(pPageTable)){
            pPageDirectory->pageDirectoryEntries[i] &= ~(unsigned int)(0x3 << 9);
        }
        else{
            // At least some things were removed
            pPageDirectory->pageDirectoryEntries[i] &= ~(unsigned int)(0x3 << 9);
            pPageDirectory->pageDirectoryEntries[i] |= (PAGE_TABLE_PARTLY_ALLOCATED_CODE << 9);
        }

        if(j!=1024){
            break;
        }
    }
}

#if E2E_TESTING
#include "../global_resources/serial_log.h"

void PageAllocator::logState(){
    SerialLog* pSerialLog = SerialLog::getSerialLog();

    pSerialLog->log((char*)"KMM: logstate begin\n");
    
    PageDirectory* pPageDirectory = pAccessor->getPageDirectory();

    for(int i=0; i<1023; i++){
        pSerialLog->log((char*)"KMM: pagedir ");
        pSerialLog->log(i);
        pSerialLog->log((char*)" ");
        int fullnessCode = (pPageDirectory->pageDirectoryEntries[i] >> 9) & 0x3;
        pSerialLog->log(fullnessCode);
        pSerialLog->log((char*)"\n");
    }

    unsigned int beginRange = 0;
    unsigned int endRange = 0;
    unsigned int currentRegionMarker = 0;
    for(unsigned int i=0; i<1023; i++){
        PageTable* pPageTable = pAccessor->getPageTable(i);
        for(unsigned int j=0; j<1024; j++){
            unsigned int regionMarker = (pPageTable->pageTableEntries[j] >> 9) & 0x3;
            if(regionMarker!=currentRegionMarker){
                unsigned int pageIndex = i*1024+j;
                if(currentRegionMarker!=0){
                    pSerialLog->log((char*)"KMM: regionend ");
                    pSerialLog->log(pageIndex-1);
                    pSerialLog->log((char*)"\n");
                }
                currentRegionMarker = regionMarker;
                if(currentRegionMarker!=0){
                    pSerialLog->log((char*)"KMM: regionbegin ");
                    pSerialLog->log(pageIndex);
                    pSerialLog->log((char*)"\n");
                }
            }
        }
    }
    if(currentRegionMarker!=0){
        pSerialLog->log((char*)"KMM: regionend ");
        pSerialLog->log(1024*1023-1);
        pSerialLog->log((char*)"\n");
    }

    pSerialLog->log((char*)"KMM: logstate end\n");
}
#endif