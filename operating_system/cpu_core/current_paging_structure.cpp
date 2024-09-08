#include "current_paging_structure.h"
#include "../cpp/placement_new.h"

extern "C" void loadPageDirectory(unsigned int physicalPageDirectoryAddress);
extern "C" void enablePaging();
extern "C" void invalidateTLB();
extern "C" void invalidatePage(unsigned int virtualPageTableAddress);
extern "C" void getPageDirectory(unsigned int* returnValue);

unsigned int CurrentPagingStructure::swapPageDirectory(unsigned int newPageDirectoryPhysicalAddr){
    unsigned int oldPhysicalPageDirectoryAddr = 0;
    getPageDirectory(&oldPhysicalPageDirectoryAddr);

    loadPageDirectory(newPageDirectoryPhysicalAddr);

    return oldPhysicalPageDirectoryAddr;
}

unsigned int CurrentPagingStructure::getPageDirectoryPhysicalAddr(){
    unsigned int oldPhysicalPageDirectoryAddr = 0;
    getPageDirectory(&oldPhysicalPageDirectoryAddr);

    return oldPhysicalPageDirectoryAddr;
}

PDE CurrentPagingStructure::getPDE(unsigned int pdeIndex){
    PDE requestedPDE;

    if(pdeIndex>=1023) return requestedPDE;

    PageTable* pHelperTable = (PageTable*)0xFFFFF000;
    PTE helperPTE = pHelperTable->getPTE(pdeIndex);

    requestedPDE.kernelPrivilegeOnly = helperPTE.kernelPrivilegeOnly;
    requestedPDE.pdeIsValid = helperPTE.pteIsValid;
    requestedPDE.pageTablePhysicalAddr = helperPTE.pagePhysicalAddr;

    return requestedPDE;
}

PTE CurrentPagingStructure::getPTE(unsigned int pdeIndex, unsigned int pteIndex){
    PTE requestedPTE;

    if(pteIndex>=1023) return requestedPTE;

    PageTable* pUsedPageTable = (PageTable*)(0xFFC00000 + pdeIndex*0x1000);
    requestedPTE = pUsedPageTable->getPTE(pteIndex);

    return requestedPTE;
}

CurrentPagingStructure::CurrentPagingStructure(unsigned int firstPageDirectoryAddr)
    :
    firstPageDirectoryAddr(firstPageDirectoryAddr)
{}

void CurrentPagingStructure::tlbInvalidatePage(Page* pPage){
    invalidatePage((unsigned int)pPage);
}

void CurrentPagingStructure::tlbInvalidateAll(){
    invalidateTLB();
}

void CurrentPagingStructure::bind(){
    loadPageDirectory(firstPageDirectoryAddr);
    enablePaging();
}