#pragma once

#include "interrupt_handler_manager.h"
#include "current_paging_structure.h"
#include "tss.h"

#include "../paging/page_allocator.h"

#include "../../cpp_lib/pair.h"
#include "../../cpp_lib/list.h"
#include "timer.h"
#include "../../cpp_lib/syscalls.h"

#if E2E_TESTING
#define NUM_POSSIBLE_TASKS (5+2)
#else
#define NUM_POSSIBLE_TASKS 50
#endif
#define MAX_TASK_ARGS 5
#define NUM_CPU_CORES 1

#define TASK_SWITCH_STACK_SIZE 1000

#define USER_STACK_SIZE 0x10000
#define KERNEL_STACK_SIZE 0x10000
#define MAIN_KERNEL_STACK 0x10FFFC

#define USER_TASK_KERNEL_STACK_OFFSET (sizeof(PageDirectory)+2*sizeof(PageTable)+KERNEL_STACK_SIZE-4)
#define USER_TASK_USER_STACK_OFFSET (sizeof(PageDirectory)+2*sizeof(PageTable)+KERNEL_STACK_SIZE+USER_STACK_SIZE-4)
#define USER_TASK_PROCESS_ENTRY_OFFSET (sizeof(PageDirectory)+2*sizeof(PageTable)+KERNEL_STACK_SIZE+USER_STACK_SIZE)

#define TASK_SWITCHING_FREQUENCY 20

typedef struct OpenSocketSyscallArgs{
    unsigned short udpPort;
    int socketID;
} OpenSocketSyscallArgs;

typedef struct SetReceiveBufferSyscallArgs{
    unsigned char socketID;
    unsigned char* buffer;
    unsigned int bufferSize;
    int success;
} SetReceiveBufferSyscallArgs;

typedef struct SetSendBufferSyscallArgs{
    unsigned char socketID;
    unsigned char* buffer;
    unsigned int bufferSize;
    int* indicatorWhenFinished;
    int success;
} SetSendBufferSyscallArgs;

typedef struct CloseSocketSyscallArgs{
    unsigned char socketID;
} CloseSocketSyscallArgs;

typedef struct PrintToScreenSyscallArgs{
    char* string;
    unsigned int stringSize;
} PrintToScreenSyscallArgs;

typedef struct GetTimerCounterSyscallArgs{
    unsigned int timerCounter;
}GetTimerSyscallArgs;

struct TaskDescriptor{
    class Task* pTask = nullptr;
    unsigned int taskEsp = 0;
    unsigned int kernelEspStackBegin = 0;
    unsigned int pageDirectoryPhysicalAddr = 0;
};

struct TaskArguments{
    unsigned int args[5];
    unsigned int numArgs = 0;
};

enum SetToRunningStateResponse{
    SUCCESS,
    TASK_ALREADY_RUNNING,
    TASK_NOT_PROPERLY_INITIALIZED,
    TOO_MANY_TASKS
};

class Task{
    protected:
        class CpuCore* pCpuCore;
        int taskID;
        bool isRunning;

    public:
        Task(class CpuCore* pCpuCore);

        int getTaskID();
        bool getRunningState();

        virtual SetToRunningStateResponse setToRunningState() = 0;
        virtual bool isKernelTask() = 0;
};

// Make it clear that these function are declared using C linkage
extern "C" void yieldTaskSwitch(unsigned int interruptParam, unsigned int* pEsp);
class CpuCore{
        friend void genericExceptionHandler(unsigned int interruptParam, unsigned int eax);
        friend void generalProtectionFaultExceptionHandler(unsigned int interruptParam, unsigned int eax);
        friend void pageFaultExceptionHandler(unsigned int interruptParam, unsigned int eax);
        friend void yieldTaskSwitch(unsigned int interruptParam, unsigned int* pEsp);
        friend void openSocketSyscallHandler(unsigned int interruptParam, unsigned int eax);
        friend void setReceiveBufferSyscallHandler(unsigned int interruptParam, unsigned int eax);
        friend void setSendBufferSyscallHandler(unsigned int interruptParam, unsigned int eax);
        friend void closeSocketSyscallHandler(unsigned int interruptParam, unsigned int eax);
        friend void printToScreenSyscallHandler(unsigned int interruptParam, unsigned int eax);
        friend void getTimerCounterSyscallHandler(unsigned int interruptParam, unsigned int eax);
        #if E2E_TESTING
        friend void debugLogInterruptHandler(unsigned int interruptParam, unsigned int eax);
        #endif

    private:
        unsigned int taskSwitchStack[TASK_SWITCH_STACK_SIZE];

        Tss tss;
        InterruptHandlerManager interruptHandlerManager;
        CurrentPagingStructure currentPagingStructure;

        bool isRunning;

        unsigned int timerCounter;

        DoublyLinkedListElement<TaskDescriptor> taskDescriptorListElements[NUM_POSSIBLE_TASKS];
        DoublyLinkedListElement<TaskDescriptor>* unusedtaskDescriptorElementsListHead;
        DoublyLinkedListElement<TaskDescriptor>* activeTasksListHead;
        DoublyLinkedListElement<TaskDescriptor>* currentTask;

        bool taskSwitchingPaused;

        class SocketManager* pSocketManager;
        unsigned int kernelPageDirectoryPhysicalAddr;
        PageAllocator* pKernelPageAlloctor;

        class TimerCallback : public Runnable{
            private:
                CpuCore* pCpuCore;

            public:
                TimerCallback(CpuCore* pCpuCore);

                void run() override;
        };

        TimerCallback timerCallback;
        Timer timer;

        static CpuCore* cpuCorePointers[NUM_CPU_CORES];
    
    public:
        class UserTask : public Task{
            private:
                unsigned int taskSpaceBeginVirtualAddr;
                unsigned int pTask8MBRegion;
                unsigned int updatedKernelStack;
                PageDirectory* pTaskPageDirectory;

            public:
                UserTask(CpuCore* pCpuCore, unsigned int taskSpaceBeginVirtualAddr);
                // Important: a task calling the destructor of the task representing itself will cause the OS to crash
                ~UserTask();

                SetToRunningStateResponse setToRunningState() override;
                bool isKernelTask() override;

                void setTaskArguments(TaskArguments& taskArgs);
                bool setData(unsigned int userAddr, unsigned char* data, unsigned int numBytes);

                // Returns:
                //  - pair.first <-> true if userAddr till userAddr+numBytes is indeed a contiguous block in userAddrSpace and is also contiguous in kernel addr space
                //  - pair.second <-> if pair.first is true, then this is the kernel address of the contiguous block
                Pair<bool, unsigned int> convertContigUserAddrBlockToContigKernelAddrBlock(unsigned int userAddr, unsigned int numBytes);
                bool addrSpaceIsUserAccessible(unsigned int userAddr, unsigned int numBytes);
        };

        class KernelTask : public Task{
            private:
                unsigned int kernelStackSpaceBegin;
                unsigned int updatedKernelStack;

            public:
                KernelTask(CpuCore* pCpuCore);
                // Important: a task calling the destructor of the task representing itself will cause the OS to crash
                ~KernelTask();

                SetToRunningStateResponse setToRunningState() override;
                bool isKernelTask() override;

                void setProcessCode(unsigned int* processCode, TaskArguments& taskArgs);
        };

        CpuCore(class SocketManager* pSocketManager, unsigned int kernelPageDirectoryPhysicalAddr, PageAllocator* pKernelPageAlloctor);
        void bind();

        Tss* getCurrentTss();
        InterruptHandlerManager* getInterruptHandlerManager();
        CurrentPagingStructure* getCurrentPagingStructure();

        // Returns 0 if successfull, 1 if not
        void withTaskSwitchingPaused(Runnable& runnable);
        
        Task* getCurrentTask();

        static CpuCore* getCpuCore(unsigned int cpuCoreId);
        static unsigned int getThisCpuCoreId();
};