#include "paging.h"

extern "C" void getCr0(unsigned int* returnValue);

unsigned int translateVirtualToPhysicalAddress(unsigned int virtualAddress){
    unsigned int cr0 = 0;
    getCr0(&cr0);

    // If paging is not enabled, virtualAddress is the physical address
    // (assuming of course segmentation is not used)
    if((cr0 & 0x80000000) == 0){
        return virtualAddress;
    }

    unsigned int pageDirectoryIndex = (virtualAddress & 0xFFC00000) >> 22;
    unsigned int pageTableIndex = (virtualAddress & 0x003FF000) >> 12;
    unsigned int pageOffset = virtualAddress & 0x00000FFF;

    // PageTable* pHelperTable = (PageTable*)0xFFFFF000;
    // pHelperTable is the 1023th page directory entry meaning it manages pages from 0xFFC00000->0xFFFFF000
    // i'th page managed by pHelperTable maps to same physical address as the i'th PageTable managed by page directory
    PageTable* pUsedPageTable = (PageTable*)(0xFFC00000 + pageDirectoryIndex*0x1000);

    return pUsedPageTable->getPTE(pageTableIndex).pagePhysicalAddr + pageOffset;
}

PageDirectory::PageDirectory(){
    unsigned int physicalPageDirectoryAddr = translateVirtualToPhysicalAddress((unsigned int)pageDirectoryEntries);

    for(int i=0; i<1023; i++){
        pageDirectoryEntries[i] = 0x00000002;
        helperTableEntries[i] = 0x00000002;
    }

    pageDirectoryEntries[1023] = (physicalPageDirectoryAddr+0x1000) | 3;
    helperTableEntries[1023] = (physicalPageDirectoryAddr+0x1000) | 3;
}

void PageDirectory::changePDE(unsigned int pdeIndex, PDE& newPDE){
    if(pdeIndex>=1023) return;

    if(newPDE.pageTablePhysicalAddr & 0xFFF) return;

    unsigned int newPdeBits = newPDE.pageTablePhysicalAddr | (((unsigned int)(!newPDE.kernelPrivilegeOnly) << 2)+((unsigned int)1 << 1)+((unsigned int)newPDE.pdeIsValid));

    pageDirectoryEntries[pdeIndex] = newPdeBits;
    helperTableEntries[pdeIndex] = newPdeBits;
}

PDE PageDirectory::getPDE(unsigned int pdeIndex){
    PDE requestedPDE;

    if(pdeIndex>=1023) return requestedPDE;

    unsigned int requestedPDEBits = pageDirectoryEntries[pdeIndex];

    requestedPDE.kernelPrivilegeOnly = !(bool)(requestedPDEBits & 0x4);
    requestedPDE.pdeIsValid = (bool)(requestedPDEBits & 0x1);
    requestedPDE.pageTablePhysicalAddr = requestedPDEBits & 0xFFFFF000;

    return requestedPDE;
}

unsigned int PageDirectory::getPhysicalAddr(){
    unsigned int physicalPageDirectoryAddr = translateVirtualToPhysicalAddress((unsigned int)pageDirectoryEntries);

    return physicalPageDirectoryAddr;
}

PageTable::PageTable(){
    for(int i=0; i<1024; i++){
        pageTableEntries[i] = 0x00000002;
    }
}

void PageTable::changePTE(unsigned int pteIndex, PTE& newPTE){
    if(pteIndex>=1024){
        return;
    }

    if(newPTE.pagePhysicalAddr & 0xFFF) return;

    unsigned int newPteBits = newPTE.pagePhysicalAddr | (((unsigned int)(!newPTE.kernelPrivilegeOnly) << 2)+((unsigned int)1 << 1)+((unsigned int)newPTE.pteIsValid));

    pageTableEntries[pteIndex] = newPteBits;
}

PTE PageTable::getPTE(unsigned int pteIndex){
    PTE requestedPTE;

    if(pteIndex>=1023) return requestedPTE;

    unsigned int requestedPTEBits = pageTableEntries[pteIndex];

    requestedPTE.kernelPrivilegeOnly = !(bool)(requestedPTEBits & 0x4);
    requestedPTE.pteIsValid = (bool)(requestedPTEBits & 0x1);
    requestedPTE.pagePhysicalAddr = requestedPTEBits & 0xFFFFF000;

    return requestedPTE;
}

unsigned int PageTable::getPhysicalAddr(){
    unsigned int physicalPageTableAddr = translateVirtualToPhysicalAddress((unsigned int)this);

    return physicalPageTableAddr;
}