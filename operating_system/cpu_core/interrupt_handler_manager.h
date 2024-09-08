#pragma once

#include "../../cpp_lib/pair.h"
#include "../../cpp_lib/callback.h"

#define IDT_ENTRIES 256

// Interrupt service routine (ISR) handler
typedef void (*IsrHandler)(unsigned int interruptParam, unsigned int eax);

enum class InterruptType{
    DivideByZero = 0,
    Debug = 1,
    NMI = 2,
    Breakpoint = 3,
    Overflow = 4,
    Bounds = 5,
    InvalidOpcode = 6,
    CoprocessorNotAvailable = 7,
    DoubleFault = 8,
    CoprocessorSegmentOverrun = 9,
    InvalidTSS = 10,
    SegmentNotPresent = 11,
    StackFault = 12,
    GeneralProtectionFault = 13,
    PageFault = 14,
    ReservedException1 = 15,
    x87FPUError = 16,
    AlignmentCheck = 17,
    MachineCheck = 18,
    SIMDException = 19,
    VirtualizationException = 20,
    ControlProtectionException = 21,
    ReservedException2 = 22,
    ReservedException3 = 23,
    ReservedException4 = 24,
    ReservedException5 = 25,
    ReservedException6 = 26,
    ReservedException7 = 27,
    HypervisorInjectionException = 28,
    VMMCommunicationException = 29,
    SecurityException = 30,
    ReservedException8 = 31,
    PITTimer = 32,
    RTCTimer = 40,
    Free1 = 41,
    Free2 = 42,
    Free3 = 43,
    Int48 = 48,
    Int49 = 49,
    Int50 = 50,
    Int51 = 51,
    Int52 = 52,
    Int53 = 53,
    Int54 = 54,
    Int55 = 55,
    Int80 = 80,
    UnknownType = 256
};

typedef struct{
	unsigned short low_offset;
	unsigned short sel;
	unsigned char always0;
	unsigned char flags;
	unsigned short high_offset;
} __attribute((packed)) IdtGate;

typedef struct{
	unsigned short limit;
	unsigned int base;
} __attribute__((packed)) IdtRegister;

class InterruptHandlerManager{
        friend class CpuCore;

    private:
        InterruptHandlerManager();
        
        void setIdtGate(int n, unsigned int handler, bool allowUserPrivilege);
        void setIdt();
        
        IdtGate idt[IDT_ENTRIES];
        IdtRegister idtReg;

        Pair<unsigned int, IsrHandler> interruptHandlers[IDT_ENTRIES];
    
    public:
        void bind();

        void withInterruptsDisabled(Runnable& runnable);

        void setInterruptHandler(InterruptType intType, const IsrHandler& newHandler);
        void setInterruptHandlerParam(InterruptType intType, unsigned int handlerParam);
        unsigned int buildIntHandlerStackForUserPriv(InterruptType intType, unsigned int kernelStack, unsigned int userStack, unsigned int processEntry);
        unsigned int buildIntHandlerStackForKernelPriv(InterruptType intType, unsigned int kernelStack, unsigned int processEntry);
};