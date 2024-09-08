#include "memory_manager.h"

MemoryManager::MemoryManager(){
    freeList = nullptr;
}

void MemoryManager::addContiguousMemoryRegion(unsigned char* pRegion, unsigned int regionSize){
    if(((unsigned int)pRegion) % 4 != 0 || regionSize % 4 != 0){
        return;
    }

    if(regionSize<=sizeof(MemoryBlockHeader)){
        return;
    }

    MemoryBlockHeader* pNewBlock = (MemoryBlockHeader*)pRegion;
    pNewBlock->size = regionSize - sizeof(MemoryBlockHeader);

    MemoryBlockHeader* pCurrentBlock = freeList;
    MemoryBlockHeader* pPreviousBlock = nullptr;
    while(pCurrentBlock != nullptr){
        if(pCurrentBlock > pNewBlock){
            break;
        }

        pPreviousBlock = pCurrentBlock;
        pCurrentBlock = pCurrentBlock->next;
    }
    
    // Here either pCurrentBlock>pNewBlock or pCurrentBlock==nullptr
    // Thus place pNewBlock after pPreviousBlock
    if(pPreviousBlock == nullptr){
        pNewBlock->next = freeList;
        freeList = pNewBlock;
    }else{
        pNewBlock->next = pPreviousBlock->next;
        pPreviousBlock->next = pNewBlock;
    }

    mergeFreeBlocks();
}

unsigned char* MemoryManager::allocate(unsigned int alignment, unsigned int size){
    if(size==0){
        return nullptr;
    }

    if((alignment & (alignment - 1)) != 0 || alignment == 0){
        // Alignment is not a power of two
        return nullptr;
    }
    
    MemoryBlockHeader* pCurrentBlock = freeList;
    MemoryBlockHeader* pPreviousBlock = nullptr;

    while(pCurrentBlock != nullptr){
        unsigned int currentBlockAddr = (unsigned int)pCurrentBlock + sizeof(MemoryBlockHeader);
        unsigned int alignedCurrentBlockAddr = (currentBlockAddr + alignment - 1) & ~(alignment - 1);
        unsigned int alignedCurrentBlockSize = pCurrentBlock->size - (alignedCurrentBlockAddr - currentBlockAddr);

        if(alignedCurrentBlockSize >= size){
            MemoryBlockHeader* possibleNextHeader = (MemoryBlockHeader*)((alignedCurrentBlockAddr + size + 3) & 0xFFFFFFFC);

            if((unsigned int)possibleNextHeader + sizeof(MemoryBlockHeader) - currentBlockAddr < pCurrentBlock->size){
                possibleNextHeader->size = pCurrentBlock->size - ((unsigned int)possibleNextHeader + sizeof(MemoryBlockHeader) - currentBlockAddr);
                possibleNextHeader->next = pCurrentBlock->next;
                if(pPreviousBlock == nullptr){
                    freeList = possibleNextHeader;
                }else{
                    pPreviousBlock->next = possibleNextHeader;
                }

                pCurrentBlock->size = (unsigned int)possibleNextHeader - currentBlockAddr;
            }
            else{
                if(pPreviousBlock == nullptr){
                    freeList = pCurrentBlock->next;
                }else{
                    pPreviousBlock->next = pCurrentBlock->next;
                }
            }

            // Zero out memory after pCurrentBlock->size and before alignedCurrentBlockAddr
            // pCurrentBlock->size can be found given alignedCurrentBlockAddr by just searching to some non-zero byte
            pCurrentBlock->next = nullptr;
            for(unsigned char* i=(unsigned char*)currentBlockAddr; i<(unsigned char*)alignedCurrentBlockAddr; i++){
                *i = 0;
            }

            return (unsigned char*)alignedCurrentBlockAddr;
        }

        pPreviousBlock = pCurrentBlock;
        pCurrentBlock = pCurrentBlock->next;
    }

    return nullptr;
}

void MemoryManager::free(unsigned char* pMemory){
    if(pMemory == nullptr){
        return;
    }

    MemoryBlockHeader* pMemoryHeader = nullptr;
    unsigned char* pMemory2 = pMemory-1;
    while(*pMemory2 == 0){
        pMemory2--;
    }
    // *pMemory2!=0 thus pMemory2 points to some byte of pMemoryHeader->size
    // Make sure that pMemory2 is 4-byte aligned
    pMemoryHeader = (MemoryBlockHeader*)(((unsigned int)pMemory2) & 0xFFFFFFFC);
    
    MemoryBlockHeader* pCurrentBlock = freeList;
    MemoryBlockHeader* pPreviousBlock = nullptr;
    while(pCurrentBlock != nullptr){
        if(pCurrentBlock > pMemoryHeader){
            break;
        }

        pPreviousBlock = pCurrentBlock;
        pCurrentBlock = pCurrentBlock->next;
    }

    // Here either pCurrentBlock>pMemoryHeader or pCurrentBlock==nullptr
    // Thus place pMemoryHeader after pPreviousBlock
    if(pPreviousBlock == nullptr){
        pMemoryHeader->next = freeList;
        freeList = pMemoryHeader;
    }else{
        pMemoryHeader->next = pPreviousBlock->next;
        pPreviousBlock->next = pMemoryHeader;
    }

    mergeFreeBlocks();
}

void MemoryManager::mergeFreeBlocks(){
    MemoryBlockHeader* pCurrentBlock = freeList;
    MemoryBlockHeader* pNextBlock = nullptr;

    while(pCurrentBlock != nullptr){
        pNextBlock = pCurrentBlock->next;
        if(pNextBlock == nullptr){
            break;
        }

        if((unsigned int)pCurrentBlock + sizeof(MemoryBlockHeader) + pCurrentBlock->size == (unsigned int)pNextBlock){
            pCurrentBlock->size += sizeof(MemoryBlockHeader) + pNextBlock->size;
            pCurrentBlock->next = pNextBlock->next;
        }else{
            pCurrentBlock = pNextBlock;
        }
    }
}
