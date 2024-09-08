#include "../cpu_core/cpu_core.h"
#include "../cpp/placement_new.h"
#include "../network_management_task/network_management_task.h"
#include "../network_management_task/network_stack_handler.h"
#include "../os_management_task/os_management_task.h"
#include "../../cpp_lib/syscalls.h"
#include "../../cpp_lib/memory_manager.h"
#include "../paging/paging_structures.h"
#include "../global_resources/bios_map.h"
#include "../global_resources/physical_network_interface.h"
#include "../global_resources/serial_log.h"
#include "../global_resources/rtc_timer.h"
#include "../global_resources/screen.h"

extern unsigned char kernel_start;
extern unsigned char kernel_end;

#define NUM_PAGE_TABLES_FOR_KERNEL 8    // kernel space goes from 0x0 -> 8*0x400000=0x2000000

#define FREE_MEMORY_ADDR ((unsigned int)(0x110000)) // Stack used by main is 0x100000 -> 0x110000
#define FREE_MEMORY_SIZE (((unsigned int)(NUM_PAGE_TABLES_FOR_KERNEL*1024*4096)) - FREE_MEMORY_ADDR)

void main(){
    MemoryManager memoryManager = MemoryManager();
    memoryManager.addContiguousMemoryRegion((unsigned char*)FREE_MEMORY_ADDR, FREE_MEMORY_SIZE);

    // Initialize global resources
    Screen::initialize(&memoryManager);
    RTCTimer::initialize(&memoryManager);
    #if E2E_TESTING
    SerialLog::initialize(&memoryManager);
    #endif
    PhysicalNetworkInterface::initialize(&memoryManager);
    BIOSMap::initialize(&memoryManager);

    // Clear the screen
    Screen* pScreen = Screen::getScreen();
    pScreen->clearScreen();
    
    // Check if the kernel is not too big
    #if E2E_TESTING
    SerialLog* pSerialLog = SerialLog::getSerialLog();
    pSerialLog->log((char*)"Main: kernelstart: 0x");
    pSerialLog->logHex((unsigned int)&kernel_start);
    pSerialLog->log((char*)"\n");
    pSerialLog->log((char*)"Main: kernelend: ");
    pSerialLog->logHex((unsigned int)&kernel_end);
    pSerialLog->log((char*)"\n");
    #endif
    if((unsigned int)(&kernel_end-&kernel_start) >= 512*200){
        pScreen->printk((char*)"Kernel is too big and overlapping with the binary for the first task!\n");
        while(1);
    }

    // Allocate memory for the kernel paging structures
    unsigned char* kernelPagingStructuresAddr = memoryManager.allocate(alignof(PagingStructures), sizeof(PagingStructures));
    if(kernelPagingStructuresAddr == nullptr){
        pScreen->printk((char*)"Failed to allocate memory for KernelPagingStructures\n");
        while(1);
    }
    PagingStructures* pKernelPagingStructures = new(kernelPagingStructuresAddr) PagingStructures();

    // Create coarsed grained memory allocator using the kernel paging structures
    // Unlike the memoryManager, this allocator is meant for allocating full pages and doesn't waste memory
    PageAllocator pageAllocator = PageAllocator((PagingStructureAccessor*)pKernelPagingStructures);
    // pages 0 <-> NUM_PAGE_TABLES_FOR_KERNEL*1024-1 are used by the kernel and partly managed by the memoryManager
    pageAllocator.allocatePageRange(0, NUM_PAGE_TABLES_FOR_KERNEL*1024-1);
    BIOSMap* pBIOSMap = BIOSMap::getBIOSMap();
    #if E2E_TESTING
    pBIOSMap->logBIOSMemoryLayout();
    #endif
    unsigned int biosMapSize = pBIOSMap->getBIOSMapSize();
    // Register the memory used by BIOS as used memory in the pageAllocator
    for(int i=0; i<biosMapSize; i++){
        BIOSMapEntry biosMapEntry = pBIOSMap->getBIOSMapEntry(i);

        // https://wiki.osdev.org/Detecting_Memory_(x86)
        if((biosMapEntry.extendedAttributes & 0x1)==0) continue;

        // https://wiki.osdev.org/Detecting_Memory_(x86)
        if(biosMapEntry.regionType==1 || biosMapEntry.regionType==3) continue;

        if(biosMapEntry.regionLength==(unsigned long long)0) continue;
        
        if(biosMapEntry.regionAddr > (unsigned long long)(LAST_ACCESSIBLE_ADDRESS)) continue;

        unsigned int firstPageIndex = ((unsigned int)biosMapEntry.regionAddr)/4096;
        unsigned int lastPageIndex = ((biosMapEntry.regionAddr+biosMapEntry.regionLength-1) > (unsigned long long)(LAST_ACCESSIBLE_ADDRESS)) ? 
                                    1024*1023-1 : ((unsigned int)(biosMapEntry.regionAddr+biosMapEntry.regionLength-1))/4096;

        pageAllocator.allocatePageRange(firstPageIndex, lastPageIndex);
    }
    // Register the memory used by the PhysicalNetworkInterface as used memory in the pageAllocator
    PhysicalNetworkInterface* pPhysicalNetworkInterface = PhysicalNetworkInterface::getPhysicalNetworkInterface();
    if(pPhysicalNetworkInterface->usesMemMappedRegisters()){
        unsigned int PhysicalNetworkInterfaceMMIOPageIndex = pPhysicalNetworkInterface->getIOBase()/0x1000;

        pageAllocator.allocatePageRange(PhysicalNetworkInterfaceMMIOPageIndex, PhysicalNetworkInterfaceMMIOPageIndex);
    }
    #if E2E_TESTING
    pageAllocator.logState();
    #endif

    // Allocate a socket manager for the cpu core (which it will use in system calls)
    unsigned char* socketManagerAddr = memoryManager.allocate(alignof(SocketManager), sizeof(SocketManager));
    if(socketManagerAddr == nullptr){
        pScreen->printk((char*)"Failed to allocate memory for SocketManager\n");
        while(1);
    }
    SocketManager* pSocketManager = new(socketManagerAddr) SocketManager();

    // Allocate a cpu core
    unsigned char* cpuCoreAddr = memoryManager.allocate(alignof(CpuCore), sizeof(CpuCore));
    if(cpuCoreAddr == nullptr){
        pScreen->printk((char*)"Failed to allocate memory for CpuCore\n");
        while(1);
    }
    CpuCore* pCpuCore = new(cpuCoreAddr) CpuCore(pSocketManager, 
        pKernelPagingStructures->getPageDirectory()->getPhysicalAddr(),
        &pageAllocator);

    // Bind to the CpuCore => sets up interrupts, paging, task switching
    pCpuCore->bind();

    class SetupKernelTasks : public Runnable{
        private:
            CpuCore* pCpuCore;
            SocketManager* pSocketManager;
            MemoryManager* pMemoryManager;

            CpuCore::KernelTask osManagementKernelTask;
            CpuCore::KernelTask networkManagementKernelTask;

        public:
            SetupKernelTasks(CpuCore* pCpuCore, SocketManager* pSocketManager, MemoryManager* pMemoryManager)
                :
                pCpuCore(pCpuCore),
                pSocketManager(pSocketManager),
                pMemoryManager(pMemoryManager),
                osManagementKernelTask(pCpuCore),
                networkManagementKernelTask(pCpuCore)
            {}

            void run() override{
                TaskArguments osManagementTaskArguments = {};
                osManagementTaskArguments.args[0] = (unsigned int)pSocketManager;
                osManagementTaskArguments.args[1] = (unsigned int)pMemoryManager;
                osManagementTaskArguments.numArgs = 2;

                unsigned int* pOsManagementTaskCode = (unsigned int*)osManagementTask;
                osManagementKernelTask.setProcessCode(pOsManagementTaskCode, osManagementTaskArguments);
                osManagementKernelTask.setToRunningState();

                TaskArguments networkManagementTaskArguments = {};
                networkManagementTaskArguments.args[0] = (unsigned int)pCpuCore;
                networkManagementTaskArguments.args[1] = (unsigned int)pSocketManager;
                networkManagementTaskArguments.args[2] = (unsigned int)pMemoryManager;
                networkManagementTaskArguments.numArgs = 3;

                unsigned int* pNetworkManagementTaskCode = (unsigned int*)networkManagementTask;
                networkManagementKernelTask.setProcessCode(pNetworkManagementTaskCode, networkManagementTaskArguments);
                networkManagementKernelTask.setToRunningState();
            }
    };

    SetupKernelTasks setupKernelTasks(pCpuCore, pSocketManager, &memoryManager);
    pCpuCore->withTaskSwitchingPaused(setupKernelTasks);

    // Go into an infinite loop, once another task takes over, we should never return here
    #if E2E_TESTING
    pSerialLog->log((char*)"Main: going into endless loop\n");
    #endif
    while(1){
        #if E2E_TESTING
        e2eTestingLog(10);
        #endif
    }
}