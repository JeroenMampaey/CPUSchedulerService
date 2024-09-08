#include "interrupt_handler_manager.h"
#include "../global_resources/io/ports.h"
#include "../../cpp_lib/atomic.h"
#include "gdt.h"

#define KERNEL_CS 0x08
#define low_16(address) (unsigned short)((address) & 0xFFFF);
#define high_16(address) (unsigned short)(((address) >> 16) & 0xFFFF);

// Exceptions
#define EXC0 0
#define EXC1 1
#define EXC2 2
#define EXC3 3
#define EXC4 4
#define EXC5 5
#define EXC6 6
#define EXC7 7
#define EXC8 8
#define EXC9 9
#define EXC10 10
#define EXC11 11
#define EXC12 12
#define EXC13 13
#define EXC14 14
#define EXC15 15
#define EXC16 16
#define EXC17 17
#define EXC18 18
#define EXC19 19
#define EXC20 20
#define EXC21 21
#define EXC22 22
#define EXC23 23
#define EXC24 24
#define EXC25 25
#define EXC26 26
#define EXC27 27
#define EXC28 28
#define EXC29 29
#define EXC30 30
#define EXC31 31

// Interrupt requests (IRQ) from PIC
#define IRQ0 32
#define IRQ1 33
#define IRQ2 34
#define IRQ3 35
#define IRQ4 36
#define IRQ5 37
#define IRQ6 38
#define IRQ7 39
#define IRQ8 40
#define IRQ9 41
#define IRQ10 42
#define IRQ11 43
#define IRQ12 44
#define IRQ13 45
#define IRQ14 46
#define IRQ15 47

// Custom defined exceptions
#define CUSTOM0 48
#define CUSTOM1 49
#define CUSTOM2 50
#define CUSTOM3 51
#define CUSTOM4 52
#define CUSTOM5 53
#define CUSTOM6 54
#define CUSTOM7 55

#define CUSTOM32 80

extern "C" void exc0();
extern "C" void exc1();
extern "C" void exc2();
extern "C" void exc3();
extern "C" void exc4();
extern "C" void exc5();
extern "C" void exc6();
extern "C" void exc7();
extern "C" void exc8();
extern "C" void exc9();
extern "C" void exc10();
extern "C" void exc11();
extern "C" void exc12();
extern "C" void exc13();
extern "C" void exc14();
extern "C" void exc15();
extern "C" void exc16();
extern "C" void exc17();
extern "C" void exc18();
extern "C" void exc19();
extern "C" void exc20();
extern "C" void exc21();
extern "C" void exc22();
extern "C" void exc23();
extern "C" void exc24();
extern "C" void exc25();
extern "C" void exc26();
extern "C" void exc27();
extern "C" void exc28();
extern "C" void exc29();
extern "C" void exc30();
extern "C" void exc31();

extern "C" void irq0();
extern "C" void irq1();
extern "C" void irq2();
extern "C" void irq3();
extern "C" void irq4();
extern "C" void irq5();
extern "C" void irq6();
extern "C" void irq7();
extern "C" void irq8();
extern "C" void irq9();
extern "C" void irq10();
extern "C" void irq11();
extern "C" void irq12();
extern "C" void irq13();
extern "C" void irq14();
extern "C" void irq15();

extern "C" void custom0();
extern "C" void custom1();
extern "C" void custom2();
extern "C" void custom3();
extern "C" void custom4();
extern "C" void custom5();
extern "C" void custom6();
extern "C" void custom7();

extern "C" void custom32();

extern "C" void createIntStackForUserPriv(unsigned int* pTopKernelStack, unsigned int kernelStack, unsigned int userStack, unsigned int processEntry, unsigned int intNumber);
extern "C" void createIntStackForKernelPriv(unsigned int* pTopKernelStack, unsigned int kernelStack, unsigned int processEntry, unsigned int intNumber);

void voidHandler(unsigned int interruptParam, unsigned eax){
    return;
}

void InterruptHandlerManager::setIdtGate(int n, unsigned int handler, bool allowUserPrivilege){
	idt[n].low_offset = low_16(handler);
	idt[n].sel = KERNEL_CS;
	idt[n].always0 = 0;
	if(allowUserPrivilege){
		idt[n].flags = 0xEE;
	}
	else{
		idt[n].flags = 0x8E;
	}
	/*
	 0x8E <--> 0x10001110

	 bit 7: interrupt is present
	 bit 6-5: privilige level of caller (0=kernel..3-user)
	 bit 4: set to 0 for interrupt gates
	 bits 3-0: 1110 <-> 14 <-> "32 bit interrupt gate"
	 */
	idt[n].high_offset = high_16(handler);
}

void InterruptHandlerManager::setIdt(){
	idtReg.base = (unsigned int) &idt;
	idtReg.limit = IDT_ENTRIES*sizeof(IdtGate)-1;
	__asm__ __volatile__("lidtl (%0)" : : "r" (&idtReg): "memory"); //apparently this is the same as "lidt [&idt_reg]"
}

InterruptHandlerManager::InterruptHandlerManager(){
    for(int i=0; i<=IDT_ENTRIES; i++){
        interruptHandlers[i] = {0, voidHandler};
    }

	setIdtGate(0, (unsigned int)exc0, false);
	setIdtGate(1, (unsigned int)exc1, false);
	setIdtGate(2, (unsigned int)exc2, false);
    setIdtGate(3, (unsigned int)exc3, false);
    setIdtGate(4, (unsigned int)exc4, false);
    setIdtGate(5, (unsigned int)exc5, false);
    setIdtGate(6, (unsigned int)exc6, false);
    setIdtGate(7, (unsigned int)exc7, false);
	setIdtGate(8, (unsigned int)exc8, false);
	setIdtGate(9, (unsigned int)exc9, false); 
	setIdtGate(10, (unsigned int)exc10, false); 
	setIdtGate(11, (unsigned int)exc11, false); 
	setIdtGate(12, (unsigned int)exc12, false);
	setIdtGate(13, (unsigned int)exc13, false); 
	setIdtGate(14, (unsigned int)exc14, false); 
	setIdtGate(15, (unsigned int)exc15, false); 
	setIdtGate(16, (unsigned int)exc16, false); 
	setIdtGate(17, (unsigned int)exc17, false); 
	setIdtGate(18, (unsigned int)exc18, false); 
	setIdtGate(19, (unsigned int)exc19, false); 
	setIdtGate(20, (unsigned int)exc20, false); 
	setIdtGate(21, (unsigned int)exc21, false); 
	setIdtGate(22, (unsigned int)exc22, false); 
	setIdtGate(23, (unsigned int)exc23, false); 
	setIdtGate(24, (unsigned int)exc24, false);
	setIdtGate(25, (unsigned int)exc25, false);
	setIdtGate(26, (unsigned int)exc26, false); 
	setIdtGate(27, (unsigned int)exc27, false); 
	setIdtGate(28, (unsigned int)exc28, false); 
	setIdtGate(29, (unsigned int)exc29, false); 
	setIdtGate(30, (unsigned int)exc30, false); 
	setIdtGate(31, (unsigned int)exc31, false);

	// Install the IRQs
    setIdtGate(32, (unsigned int)irq0, true);
    setIdtGate(33, (unsigned int)irq1, true);
    setIdtGate(34, (unsigned int)irq2, true);
    setIdtGate(35, (unsigned int)irq3, true);
    setIdtGate(36, (unsigned int)irq4, true);
    setIdtGate(37, (unsigned int)irq5, true);
    setIdtGate(38, (unsigned int)irq6, true);
    setIdtGate(39, (unsigned int)irq7, true);
    setIdtGate(40, (unsigned int)irq8, true);
    setIdtGate(41, (unsigned int)irq9, true);
    setIdtGate(42, (unsigned int)irq10, true);
    setIdtGate(43, (unsigned int)irq11, true);
    setIdtGate(44, (unsigned int)irq12, true);
    setIdtGate(45, (unsigned int)irq13, true);
    setIdtGate(46, (unsigned int)irq14, true);
    setIdtGate(47, (unsigned int)irq15, true);

	setIdtGate(48, (unsigned int)custom0, true);
	setIdtGate(49, (unsigned int)custom1, true);
	setIdtGate(50, (unsigned int)custom2, true);
	setIdtGate(51, (unsigned int)custom3, true);
    setIdtGate(52, (unsigned int)custom4, true);
	setIdtGate(53, (unsigned int)custom5, true);
    setIdtGate(54, (unsigned int)custom6, true);
    setIdtGate(55, (unsigned int)custom7, true);

    setIdtGate(80, (unsigned int)custom32, false);
}

void InterruptHandlerManager::setInterruptHandler(InterruptType intType, const IsrHandler& newHandler){
    unsigned int intTypeToInteger = (unsigned int)intType;
    
    if(intTypeToInteger > CUSTOM7 && intTypeToInteger != CUSTOM32){
        return;
    }

    atomicStore((unsigned int*)(&interruptHandlers[intTypeToInteger].second), (unsigned int)newHandler);
}

void InterruptHandlerManager::setInterruptHandlerParam(InterruptType intType, unsigned int handlerParam){
    unsigned int intTypeToInteger = (unsigned int)intType;
    
    if(intTypeToInteger > CUSTOM7 && intTypeToInteger != CUSTOM32){
        return;
    }

    atomicStore((unsigned int*)(&interruptHandlers[intTypeToInteger].first), handlerParam);
}

unsigned int InterruptHandlerManager::buildIntHandlerStackForUserPriv(InterruptType intType, unsigned int kernelStack, unsigned int userStack, unsigned int processEntry){
    unsigned int intTypeToInteger = (unsigned int)intType;
    unsigned int topKernelStack = 0;
    
    if(intTypeToInteger > CUSTOM7 && intTypeToInteger != CUSTOM32){
        return topKernelStack;
    }

    createIntStackForUserPriv(&topKernelStack, kernelStack, userStack, processEntry, intTypeToInteger);

    return topKernelStack;
}

unsigned int InterruptHandlerManager::buildIntHandlerStackForKernelPriv(InterruptType intType, unsigned int kernelStack, unsigned int processEntry){
    unsigned int intTypeToInteger = (unsigned int)intType;
    unsigned int topKernelStack = 0;
    
    if(intTypeToInteger > CUSTOM7 && intTypeToInteger != CUSTOM32){
        return topKernelStack;
    }

    createIntStackForKernelPriv(&topKernelStack, kernelStack, processEntry, intTypeToInteger);

    return topKernelStack;
}

void InterruptHandlerManager::bind(){
    // Set the INTERRUPT_HANDLERS gdt entry to point to the interrupt handlers
    // The fs segment register points to this gdt entry
    unsigned int limit = sizeof(Pair<unsigned int, IsrHandler>)*IDT_ENTRIES;
    unsigned int base = (unsigned int)&interruptHandlers;
    GdtEntry* gdtEntries = getGdtEntries();
    gdtEntries[6].base_low = base & 0xFFFF;
    gdtEntries[6].base_middle = (base >> 16) & 0xFF;
    gdtEntries[6].base_high = (base >> 24) & 0xFF;
    gdtEntries[6].limit_low = limit & 0xFFFF;
    gdtEntries[6].granularity = gdtEntries[6].granularity | ((limit >> 16) & 0x0F);

    // Remap the PIC
	//The Master PIC has command 0x20 and data 0x21, while the slave has command 0xA0 and data 0xA1
	//https://wiki.osdev.org/PIC
    portByteOut(0x20, 0x11);
    portByteOut(0xA0, 0x11);
    portByteOut(0x21, 0x20);
    portByteOut(0xA1, 0x28);
    portByteOut(0x21, 0x04);
    portByteOut(0xA1, 0x02);
    portByteOut(0x21, 0x01);
    portByteOut(0xA1, 0x01);
    portByteOut(0x21, 0x0);
    portByteOut(0xA1, 0x0);
	
	setIdt();

    __asm__ __volatile__("sti" ::: "memory");
}

void InterruptHandlerManager::withInterruptsDisabled(Runnable& runnable){
    __asm__ __volatile__("cli" ::: "memory");
    runnable.run();
    __asm__ __volatile__("sti" ::: "memory");
}