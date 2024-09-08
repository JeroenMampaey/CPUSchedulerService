#pragma once

typedef struct TssEntry {
    unsigned int prevTss;
    unsigned int esp0;
    unsigned int ss0;
    unsigned int esp1;
    unsigned int ss1;
    unsigned int esp2;
    unsigned int ss2;
    unsigned int cr3;
    unsigned int eip;
    unsigned int eflags;
    unsigned int eax;
    unsigned int ecx;
    unsigned int edx;
    unsigned int ebx;
    unsigned int esp;
    unsigned int ebp;
    unsigned int esi;
    unsigned int edi;
    unsigned int es;
    unsigned int cs;
    unsigned int ss;
    unsigned int ds;
    unsigned int fs;
    unsigned int gs;
    unsigned int ldt;
    unsigned short trap;
    unsigned short iomap;
} __attribute__((packed)) TssEntry;

class Tss{
        friend class CpuCore;

    private:
        Tss(unsigned int firstEsp0Value);

        TssEntry tssEntry;

    public:
        void bind();
        void setEsp0(unsigned int newEsp0);
};