#pragma once

typedef struct Page{
    unsigned int pageEntries[1024];
} __attribute__((packed)) Page __attribute__((aligned(4096)));

typedef struct PDE{
    unsigned int pageTablePhysicalAddr;
    bool kernelPrivilegeOnly;
    bool pdeIsValid;
} PDE;

typedef struct PTE{
    unsigned int pagePhysicalAddr;
    bool kernelPrivilegeOnly;
    bool pteIsValid;
} PTE;

class PageDirectory{
        friend class PageAllocator;
    public:
        PageDirectory();
        void changePDE(unsigned int pdeIndex, PDE& newPDE);
        PDE getPDE(unsigned int pdeIndex);
        unsigned int getPhysicalAddr();

    private:
        unsigned int pageDirectoryEntries[1024];
        unsigned int helperTableEntries[1024];
} __attribute__((packed)) __attribute__((aligned(4096)));

class PageTable{
        friend class PageAllocator;
    public:
        PageTable();
        void changePTE(unsigned int pteIndex, PTE& newPTE);
        PTE getPTE(unsigned int pteIndex);
        unsigned int getPhysicalAddr();

    private:
        unsigned int pageTableEntries[1024];
}__attribute__((packed)) __attribute__((aligned(4096)));