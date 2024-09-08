#include "gdt.h"

GdtEntry* getGdtEntries(){
    GdtDescr gdtDescr;

    __asm__ __volatile__("sgdt %0" :"=m"(gdtDescr));

    return (GdtEntry*)gdtDescr.base;
}