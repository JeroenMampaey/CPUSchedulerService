#pragma once

class MemoryManager{
    private:
        // This structure will require a 4-byte alignment
        struct MemoryBlockHeader{
            unsigned int size;
            MemoryBlockHeader* next;
        };

        MemoryBlockHeader* freeList;

        void mergeFreeBlocks();

    public:
        MemoryManager();

        // Adds a contiguous memory region to the memory manager's free list
        // pRegion should be 4-byte aligned, and regionSize should be a multiple of 4
        void addContiguousMemoryRegion(unsigned char* pRegion, unsigned int regionSize);

        // Allocates memory of the given size with the specified alignment
        // alignment must be a power of 2; returns nullptr if allocation fails
        unsigned char* allocate(unsigned int alignment, unsigned int size);

        // Frees previously allocated memory. Passing nullptr is allowed and will be ignored.
        // However, freeing the same memory block twice will lead to undefined behavior.
        void free(unsigned char* pMemory);
};