#include "paging_structures.h"
#include "../../cpp_lib/placement_new.h"
#include "../global_resources/screen.h"

PagingStructures::PagingStructures(){
    Screen* pScreen = Screen::getScreen();

    if(sizeof(PageDirectory)!=8192){
        pScreen->printk((char*)"Sizeof page directory is unexpected\n");
        while(1);
    }

    new((unsigned char*)&pageDirectory) PageDirectory();

    if(sizeof(PageTable)!=4096){
        pScreen->printk((char*)"Sizeof page table is unexpected\n");
        while(1);
    }

    for(unsigned int i=0; i<1023; i++){
        PageTable* pPageTable = new((unsigned char*)&pageTables[i]) PageTable();

        for(unsigned int j = 0; j < 1024; j++){
            PTE newPTE = {(unsigned int)(i*(0x1000*1024)+j*0x1000), true, true};
            pPageTable->changePTE(j, newPTE);
        }

        PDE newPDE = {(unsigned int)pPageTable, true, true};
        ((PageDirectory*)&pageDirectory)->changePDE(i, newPDE);
    }
}

PageDirectory* PagingStructures::getPageDirectory(){
    return (PageDirectory*)(&pageDirectory);
}

PageTable* PagingStructures::getPageTable(unsigned int i){
    if(i<1023){
        return (PageTable*)(&pageTables[i]);
    }

    return nullptr;
}