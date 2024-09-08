#include "tss.h"
#include "../../cpp_lib/mem.h"
#include "gdt.h"

extern "C" void flush_tss();

Tss::Tss(unsigned int firstEsp0Value){
    tssEntry.esp0 = firstEsp0Value;
}

void Tss::setEsp0(unsigned int newEsp0){
    tssEntry.esp0 = newEsp0;
}

void Tss::bind(){
    GdtEntry* gdtEntries = getGdtEntries();

    unsigned int limit = sizeof(TssEntry);
    unsigned int base = (unsigned int)&tssEntry;
    memClear((unsigned char*)base, limit);
    unsigned int ssValue = (2 * 8);
    __asm__ __volatile__("movl %%ss, %0;" : "=r" (ssValue) : );
    tssEntry.ss0 = ssValue;
    gdtEntries[5].base_low = base & 0xFFFF;
    gdtEntries[5].base_middle = (base >> 16) & 0xFF;
    gdtEntries[5].base_high = (base >> 24 & 0xFF);
    gdtEntries[5].limit_low = limit & 0xFFFF;
    gdtEntries[5].granularity = gdtEntries[5].granularity | ((limit >> 16) & 0x0F);

    flush_tss();

    /*Screen* pScreen = getScreen();
    
    pScreen->printk((char*)"Gdt base address: ");
    pScreen->printk(gdtTest.base);
    pScreen->printk((char*)"\n");

    pScreen->printk((char*)"Gdt limit: ");
    pScreen->printk(gdtTest.limit);
    pScreen->printk((char*)"\n");*/
}