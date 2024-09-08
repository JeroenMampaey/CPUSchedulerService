#pragma once

typedef struct GdtDescr{
    unsigned short limit;
    unsigned int base;
} __attribute__((packed)) GdtDescr; 

typedef struct GdtEntry{
    unsigned short limit_low;
    unsigned short base_low;
    unsigned char  base_middle;
    unsigned char  access;
    unsigned char  granularity;
    unsigned char  base_high;
} __attribute__((packed)) GdtEntry;

GdtEntry* getGdtEntries();