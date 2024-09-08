#include "cpu_core.h"
#include "../global_resources/serial_log.h"
#include "../global_resources/screen.h"
#include "../cpp/placement_new.h"
#include "../../cpp_lib/mem.h"
#include "../../cpp_lib/atomic.h"
#include "../../cpp_lib/syscalls.h"
#include "../network_management_task/socket_manager.h"
#include "../../cpp_lib/callback.h"

// This assembly code will call the yieldTaskSwitch function
extern "C" void yieldTaskSwitchIntHandler(unsigned int interruptParam, unsigned int eax);

void genericExceptionHandler(unsigned int interruptParam, unsigned int eax){
    yield();
}

void generalProtectionFaultExceptionHandler(unsigned int interruptParam, unsigned int eax){
    #if E2E_TESTING
    SerialLog* pSerialLog = SerialLog::getSerialLog();
    pSerialLog->log((char*)"Exception: 13\n");
    #endif

    Screen* pScreen = Screen::getScreen();
    pScreen->printk((char*)"General Protection Fault interrupt was called\n");

    genericExceptionHandler(interruptParam, eax);
}

void pageFaultExceptionHandler(unsigned int interruptParam, unsigned int eax){
    #if E2E_TESTING
    SerialLog* pSerialLog = SerialLog::getSerialLog();
    pSerialLog->log((char*)"Exception: 14\n");
    #endif

    Screen* pScreen = Screen::getScreen();
    pScreen->printk((char*)"Page Fault interrupt was called\n");
    
    genericExceptionHandler(interruptParam, eax);
}

extern "C" void yieldTaskSwitch(unsigned int interruptParam, unsigned int* pEsp){
    CpuCore* pCpuCore = (CpuCore*)interruptParam;

    if(pCpuCore->taskSwitchingPaused || pCpuCore->activeTasksListHead == nullptr){
        return;
    }

    DoublyLinkedListElement<TaskDescriptor>* currentTask = pCpuCore->currentTask;
    DoublyLinkedListElement<TaskDescriptor>* nextTask = (currentTask==nullptr || currentTask->next==nullptr) ? pCpuCore->activeTasksListHead : currentTask->next;

    if(currentTask == nextTask){
        return;
    }

    pCpuCore->currentTask = nextTask;

    if(currentTask != nullptr){
        currentTask->value.taskEsp = *pEsp;
    }

    *pEsp = nextTask->value.taskEsp;
    pCpuCore->tss.setEsp0(nextTask->value.kernelEspStackBegin);

    unsigned int newPageDirectoryPhysicalAddr = nextTask->value.pageDirectoryPhysicalAddr;

    if(currentTask != nullptr){
        currentTask->value.pageDirectoryPhysicalAddr = pCpuCore->currentPagingStructure.swapPageDirectory(newPageDirectoryPhysicalAddr);
    }
    else{
        pCpuCore->currentPagingStructure.swapPageDirectory(newPageDirectoryPhysicalAddr);
    }
}

void openSocketSyscallHandler(unsigned int interruptParam, unsigned int eax){
    // First, make sure that eax points to some space accessible by the task
    CpuCore* pCpuCore = (CpuCore*)interruptParam;
    Task* pTask = pCpuCore->getCurrentTask();
    if(!pTask->isKernelTask()){
        CpuCore::UserTask* pUserTask = (CpuCore::UserTask*)pTask;
        if(!pUserTask->addrSpaceIsUserAccessible(eax, sizeof(OpenSocketSyscallArgs))){
            return;
        }
    }
    
    OpenSocketSyscallArgs* pOpenSocketSyscallArgs = (OpenSocketSyscallArgs*)eax;
    SocketManager* pSocketManager = pCpuCore->pSocketManager;

    // It is impossible that taskID should be -1 here since the task is running
    unsigned char taskId = pTask->getTaskID();
    pOpenSocketSyscallArgs->socketID = pSocketManager->openSocket(taskId, pOpenSocketSyscallArgs->udpPort);
}

void setReceiveBufferSyscallHandler(unsigned int interruptParam, unsigned int eax){
    CpuCore* pCpuCore = (CpuCore*)interruptParam;
    Task* pTask = pCpuCore->getCurrentTask();

    // First, make sure that eax points to some space accessible by the task
    if(!pTask->isKernelTask()){
        CpuCore::UserTask* pUserTask = (CpuCore::UserTask*)pTask;
        if(!pUserTask->addrSpaceIsUserAccessible(eax, sizeof(SetReceiveBufferSyscallArgs))){
            return;
        }
    }

    SetReceiveBufferSyscallArgs* pSetReceiveBufferSyscallArgs = (SetReceiveBufferSyscallArgs*)eax;
    SocketManager* pSocketManager = pCpuCore->pSocketManager;

    // It is impossible that taskID should be -1 here since the task is running
    unsigned char taskId = (unsigned char)pTask->getTaskID();

    // Now there is an important little detail about pSetReceiveBufferSyscallArgs->buffer:
    //
    // pSetReceiveBufferSyscallArgs->buffer is the address of the buffer according to the page directory of the task calling 
    // the syscall => if the task is a user task then the address according to the kernel page directory might be different
    //
    // Thus we need to check if this is a kernel task, if not then we need to translate the buffer address to the address
    // in kernel space.
    //
    // Also we need to check if the buffer is indeed in the user space!!! otherwise user tasks might abuse this 
    // to ruin the kernel space or space of other tasks
    if(pTask->isKernelTask()){
        pSetReceiveBufferSyscallArgs->success = pSocketManager->setReceiveBuffer(
            taskId, pSetReceiveBufferSyscallArgs->socketID, pSetReceiveBufferSyscallArgs->buffer, 
            pSetReceiveBufferSyscallArgs->bufferSize);
    }
    else{
        CpuCore::UserTask* pUserTask = (CpuCore::UserTask*)pTask;

        Pair<bool, unsigned int> convertedAddrBlock = pUserTask->convertContigUserAddrBlockToContigKernelAddrBlock(
            (unsigned int)pSetReceiveBufferSyscallArgs->buffer, pSetReceiveBufferSyscallArgs->bufferSize);
        
        if(!convertedAddrBlock.first){
            pSetReceiveBufferSyscallArgs->success = -1;
            return;
        }

        pSetReceiveBufferSyscallArgs->success = pSocketManager->setReceiveBuffer(
            taskId, pSetReceiveBufferSyscallArgs->socketID, (unsigned char*)convertedAddrBlock.second, 
            pSetReceiveBufferSyscallArgs->bufferSize);
    }
}

void setSendBufferSyscallHandler(unsigned int interruptParam, unsigned int eax){
    CpuCore* pCpuCore = (CpuCore*)interruptParam;
    Task* pTask = pCpuCore->getCurrentTask();

    // First, make sure that eax points to some space accessible by the task
    if(!pTask->isKernelTask()){
        CpuCore::UserTask* pUserTask = (CpuCore::UserTask*)pTask;
        if(!pUserTask->addrSpaceIsUserAccessible(eax, sizeof(SetSendBufferSyscallArgs))){
            return;
        }
    }

    SetSendBufferSyscallArgs* pSetSendBufferSyscallArgs = (SetSendBufferSyscallArgs*)eax;
    SocketManager* pSocketManager = pCpuCore->pSocketManager;

    // It is impossible that taskID should be -1 here since the task is running
    unsigned char taskId = (unsigned char)pTask->getTaskID();

    // Now there is an important little detail about pSetSendBufferSyscallArgs->buffer:
    //
    // pSetSendBufferSyscallArgs->buffer is the address of the buffer according to the page directory of the task calling 
    // the syscall => if the task is a user task then the address according to the kernel page directory might be different
    //
    // Thus we need to check if this is a kernel task, if not then we need to translate the buffer address to the address
    // in kernel space.
    //
    // Also we need to check if the buffer is indeed in the user space!!! otherwise user tasks might abuse this 
    // to ruin the kernel space or space of other tasks
    //
    // Another small details is that the exact same goes for the indicatorWhenFinished
    if(pTask->isKernelTask()){
        pSetSendBufferSyscallArgs->success = pSocketManager->setSendBuffer(
            taskId, pSetSendBufferSyscallArgs->socketID, pSetSendBufferSyscallArgs->buffer, 
            pSetSendBufferSyscallArgs->bufferSize, pSetSendBufferSyscallArgs->indicatorWhenFinished);
    }
    else{
        CpuCore::UserTask* pUserTask = (CpuCore::UserTask*)pTask;

        Pair<bool, unsigned int> convertedAddrBlock = pUserTask->convertContigUserAddrBlockToContigKernelAddrBlock(
            (unsigned int)pSetSendBufferSyscallArgs->buffer, pSetSendBufferSyscallArgs->bufferSize);
        Pair<bool, unsigned int> convertedAddrBlock2 = pUserTask->convertContigUserAddrBlockToContigKernelAddrBlock(
            (unsigned int)pSetSendBufferSyscallArgs->indicatorWhenFinished, sizeof(int));
        
        if(!convertedAddrBlock.first || !convertedAddrBlock2.first){
            pSetSendBufferSyscallArgs->success = -1;
            return;
        }
        
        pSetSendBufferSyscallArgs->success = pSocketManager->setSendBuffer(
            taskId, pSetSendBufferSyscallArgs->socketID, (unsigned char*)convertedAddrBlock.second, 
            pSetSendBufferSyscallArgs->bufferSize, (int*)convertedAddrBlock2.second);
    }
}

void closeSocketSyscallHandler(unsigned int interruptParam, unsigned int eax){
    CpuCore* pCpuCore = (CpuCore*)interruptParam;
    Task* pTask = pCpuCore->getCurrentTask();

    // First, make sure that eax points to some space accessible by the task
    if(!pTask->isKernelTask()){
        CpuCore::UserTask* pUserTask = (CpuCore::UserTask*)pTask;
        if(!pUserTask->addrSpaceIsUserAccessible(eax, sizeof(CloseSocketSyscallArgs))){
            return;
        }
    }
    
    CloseSocketSyscallArgs* pCloseSocketSyscallArgs = (CloseSocketSyscallArgs*)eax;
    SocketManager* pSocketManager = pCpuCore->pSocketManager;

    // It is impossible that taskID should be -1 here since the task is running
    unsigned char taskId = (unsigned char)pCpuCore->getCurrentTask()->getTaskID();
    pSocketManager->closeSocket(taskId, pCloseSocketSyscallArgs->socketID);
}

void printToScreenSyscallHandler(unsigned int interruptParam, unsigned int eax){
    CpuCore* pCpuCore = (CpuCore*)interruptParam;
    Task* pTask = pCpuCore->getCurrentTask();

    // First, make sure that eax points to some space accessible by the task
    if(!pTask->isKernelTask()){
        CpuCore::UserTask* pUserTask = (CpuCore::UserTask*)pTask;
        if(!pUserTask->addrSpaceIsUserAccessible(eax, sizeof(PrintToScreenSyscallArgs))){
            return;
        }
    }

    PrintToScreenSyscallArgs* pPrintToScreenSyscallArgs = (PrintToScreenSyscallArgs*)eax;

    // Then, make sure that this task is allowed access to the string from the argument
    if(!pTask->isKernelTask()){
        CpuCore::UserTask* pUserTask = (CpuCore::UserTask*)pTask;
        if(!pUserTask->addrSpaceIsUserAccessible((unsigned int)pPrintToScreenSyscallArgs->string, pPrintToScreenSyscallArgs->stringSize+1)){
            return;
        }
    }

    // Now make sure that the string is null terminated
    if(pPrintToScreenSyscallArgs->string[pPrintToScreenSyscallArgs->stringSize] != '\0'){
        return;
    }

    Screen* pScreen = Screen::getScreen();
    pScreen->printk(pPrintToScreenSyscallArgs->string);
}

void getTimerCounterSyscallHandler(unsigned int interruptParam, unsigned int eax){
    GetTimerCounterSyscallArgs* pGetTimerCounterSyscallArgs = (GetTimerCounterSyscallArgs*)eax;

    // First, make sure that eax points to some space accessible by the task
    CpuCore* pCpuCore = (CpuCore*)interruptParam;
    Task* pTask = pCpuCore->getCurrentTask();
    if(!pTask->isKernelTask()){
        CpuCore::UserTask* pUserTask = (CpuCore::UserTask*)pTask;
        if(!pUserTask->addrSpaceIsUserAccessible(eax, sizeof(GetTimerCounterSyscallArgs))){
            return;
        }
    }

    pGetTimerCounterSyscallArgs->timerCounter = pCpuCore->timerCounter;
}

#if E2E_TESTING
void debugLogInterruptHandler(unsigned int interruptParam, unsigned int eax){
    SerialLog* pSerialLog = SerialLog::getSerialLog();
    pSerialLog->log((char*)"LogInt: ");
    pSerialLog->log(eax);
    pSerialLog->log((char*)"\n");
}
#endif

CpuCore* CpuCore::cpuCorePointers[1];

unsigned int CpuCore::getThisCpuCoreId(){
    // Since we are only working with a single core at the moment, this will suffice
    // CPUID instruction will need to be used when working in multi-core environment
    return 0;
}

CpuCore::CpuCore(SocketManager* pSocketManager, unsigned int kernelPageDirectoryPhysicalAddr, PageAllocator* pKernelPageAlloctor)
    :
    tss((unsigned int)MAIN_KERNEL_STACK),
    interruptHandlerManager(),
    currentPagingStructure(kernelPageDirectoryPhysicalAddr),
    isRunning(false),
    timerCounter(0),
    unusedtaskDescriptorElementsListHead(nullptr),
    activeTasksListHead(nullptr),
    currentTask(nullptr),
    taskSwitchingPaused(false),
    pSocketManager(pSocketManager),
    kernelPageDirectoryPhysicalAddr(kernelPageDirectoryPhysicalAddr),
    pKernelPageAlloctor(pKernelPageAlloctor),
    timer(TASK_SWITCHING_FREQUENCY),
    timerCallback(this)
{
    #if E2E_TESTING
    SerialLog* pSerialLog = SerialLog::getSerialLog();
    pSerialLog->log((char*)"CpuCore: constructor\n");
    #endif

    if((unsigned int)this != (unsigned int)taskSwitchStack){
        Screen* pScreen = Screen::getScreen();
        pScreen->printk((char*)"It is assumed the compiler should place CpuCore and CpuCore::taskSwitchStack at the same address but this isn't the case! OS won't start...\n");
        while(true);
    }

    for(unsigned int i=0; i<NUM_POSSIBLE_TASKS; i++){
        if(i==NUM_POSSIBLE_TASKS-1){
            taskDescriptorListElements[i].next = nullptr;
        }
        else{
            taskDescriptorListElements[i].next = &taskDescriptorListElements[i+1];
        }
    }
    unusedtaskDescriptorElementsListHead = &taskDescriptorListElements[0];
    
    // Setup some interrupt handlers specific for this cpu core meaning:
    // - Exceptions
    // - Default PIC interrupt handlers
    // - Syscalls
    for(unsigned int intType=(unsigned int)InterruptType::DivideByZero; 
        intType<=(unsigned int)InterruptType::ReservedException8;
        intType++)
    {
        if(intType!=(unsigned int)InterruptType::GeneralProtectionFault && intType!=(unsigned int)InterruptType::PageFault){
            interruptHandlerManager.setInterruptHandlerParam((InterruptType)intType, (unsigned int)this);
            interruptHandlerManager.setInterruptHandler((InterruptType)intType, genericExceptionHandler);
        }
    }
    interruptHandlerManager.setInterruptHandlerParam(InterruptType::GeneralProtectionFault, (unsigned int)this);
    interruptHandlerManager.setInterruptHandler(InterruptType::GeneralProtectionFault, generalProtectionFaultExceptionHandler);

    interruptHandlerManager.setInterruptHandlerParam(InterruptType::PageFault, (unsigned int)this);
    interruptHandlerManager.setInterruptHandler(InterruptType::PageFault, pageFaultExceptionHandler);

    interruptHandlerManager.setInterruptHandlerParam(InterruptType::Int49, (unsigned int)this);
    interruptHandlerManager.setInterruptHandler(InterruptType::Int49, yieldTaskSwitchIntHandler);

    interruptHandlerManager.setInterruptHandlerParam(InterruptType::Int50, (unsigned int)this);
    interruptHandlerManager.setInterruptHandler(InterruptType::Int50, openSocketSyscallHandler);

    interruptHandlerManager.setInterruptHandlerParam(InterruptType::Int51, (unsigned int)this);
    interruptHandlerManager.setInterruptHandler(InterruptType::Int51, setReceiveBufferSyscallHandler);

    interruptHandlerManager.setInterruptHandlerParam(InterruptType::Int52, (unsigned int)this);
    interruptHandlerManager.setInterruptHandler(InterruptType::Int52, setSendBufferSyscallHandler);
    
    interruptHandlerManager.setInterruptHandlerParam(InterruptType::Int53, (unsigned int)this);
    interruptHandlerManager.setInterruptHandler(InterruptType::Int53, closeSocketSyscallHandler);

    interruptHandlerManager.setInterruptHandlerParam(InterruptType::Int54, (unsigned int)this);
    interruptHandlerManager.setInterruptHandler(InterruptType::Int54, printToScreenSyscallHandler);

    interruptHandlerManager.setInterruptHandlerParam(InterruptType::Int55, (unsigned int)this);
    interruptHandlerManager.setInterruptHandler(InterruptType::Int55, getTimerCounterSyscallHandler);

    #if E2E_TESTING
    interruptHandlerManager.setInterruptHandlerParam(InterruptType::Int48, (unsigned int)this);
    interruptHandlerManager.setInterruptHandler(InterruptType::Int48, debugLogInterruptHandler);
    #endif
}

void CpuCore::bind(){
    // Indicate that the CpuCore object responsible for handling this actual cpu core is "this"
    unsigned int thisCpuCoreId = getThisCpuCoreId();
    if(thisCpuCoreId>=1){
        Screen* pScreen = Screen::getScreen();
        pScreen->printk((char*)"In CpuCore::bind(), getThisCpuCoreId() returned ");
        pScreen->printk(thisCpuCoreId);
        pScreen->printk((char*)" which is unexpected!\n");
        while(1);
    }
    CpuCore::cpuCorePointers[thisCpuCoreId] = this;

    // Bind cpu core private resources to this cpu core
    timer.bind();
    tss.bind();
    currentPagingStructure.bind();
    interruptHandlerManager.bind();

    // Indicate that the core is running
    isRunning = true;
    
    // Only now bind task switching to the timer interrupt handler
    timer.setTimerCallback(&timerCallback);
}

CpuCore* CpuCore::getCpuCore(unsigned int cpuCoreId){
    if(cpuCoreId==0){
        return CpuCore::cpuCorePointers[0];
    }

    return nullptr;
}

InterruptHandlerManager* CpuCore::getInterruptHandlerManager(){
    return &interruptHandlerManager;
}

Tss* CpuCore::getCurrentTss(){
    return &tss;
}

CurrentPagingStructure* CpuCore::getCurrentPagingStructure(){
    return &currentPagingStructure;
}

void CpuCore::withTaskSwitchingPaused(Runnable& runnable){
    taskSwitchingPaused = true;
    runnable.run();
    taskSwitchingPaused = false;
}

Task* CpuCore::getCurrentTask(){
    return currentTask->value.pTask;
}

Task::Task(CpuCore* pCpuCore)
    :
    pCpuCore(pCpuCore),
    taskID(-1),
    isRunning(false)
{}

int Task::getTaskID(){
    return taskID;
}

bool Task::getRunningState(){
    return isRunning;
}

CpuCore::UserTask::UserTask(CpuCore* pCpuCore, unsigned int taskSpaceBeginVirtualAddr)
    :
    Task(pCpuCore),
    taskSpaceBeginVirtualAddr(taskSpaceBeginVirtualAddr)
{
    pTask8MBRegion = 0;

    if(pCpuCore->isRunning 
        && pCpuCore->currentPagingStructure.getPageDirectoryPhysicalAddr()==pCpuCore->kernelPageDirectoryPhysicalAddr
        && (taskSpaceBeginVirtualAddr % (4096*1024))==0){
        pTask8MBRegion = pCpuCore->pKernelPageAlloctor->allocateContiguousPages(1024*2);
    }

    if(pTask8MBRegion!=0){
        // taskSpaceBeginVirtualAddr should be 4MB alligned, a.k.a a multiple of 1024*4096
        unsigned int taskFirstPdeIndex = taskSpaceBeginVirtualAddr/(1024*4096);
        unsigned int taskSecondPdeIndex = taskFirstPdeIndex+1;
        // pTaskRegion should be page alligned, a.k.a a multiple of 4096
        unsigned int taskRegionPageIndex = (pTask8MBRegion)/4096;

        unsigned int currentPage = pTask8MBRegion;

        // First create a page directory which is exactly the same as the kernel page directory
        // Allocated at the begin of the 8MB region
        pTaskPageDirectory = new((unsigned char*)currentPage) PageDirectory();
        for(int i=0; i<1023; i++){
            PDE pde = pCpuCore->currentPagingStructure.getPDE(i);
            if(pde.pdeIsValid){
                PDE taskPde = {pde.pageTablePhysicalAddr, pde.kernelPrivilegeOnly, true};
                pTaskPageDirectory->changePDE(i, taskPde);
            }
        }

        // Create a page table for the first 4MB of the 8MB region
        currentPage += sizeof(PageDirectory);
        PageTable* taskPageTable1 = new((unsigned char*)currentPage) PageTable();
        // 2 of the first 4 pages are for new page directory, 2 are page tables for the 8MB Region
        // Thus these pages should only be accessible (read/write) by the kernel
        for(int i=0; i<4; i++){
            PTE pte = pCpuCore->currentPagingStructure.getPTE((taskRegionPageIndex+i)/1024, (taskRegionPageIndex+i) % 1024);
            PTE taskPTE = {pte.pagePhysicalAddr, true, true};
            taskPageTable1->changePTE(i, taskPTE);
        }
        // Each task has a kernel stack which is used when the task goes from user to kernel mode
        // These pages should also only be accessible by the kernel
        for(int i=4; i<4+(KERNEL_STACK_SIZE/0x1000); i++){
            PTE pte = pCpuCore->currentPagingStructure.getPTE((taskRegionPageIndex+i)/1024, (taskRegionPageIndex+i) % 1024);
            PTE taskPTE = {pte.pagePhysicalAddr, true, true};
            taskPageTable1->changePTE(i, taskPTE);
        }
        // The rest of the pages are for the user space
        for(int i=4+(KERNEL_STACK_SIZE/0x1000); i<1024; i++){
            PTE pte = pCpuCore->currentPagingStructure.getPTE((taskRegionPageIndex+i)/1024, (taskRegionPageIndex+i) % 1024);
            PTE taskPTE = {pte.pagePhysicalAddr, false, true};
            taskPageTable1->changePTE(i, taskPTE);
        }
        PDE taskPde = {taskPageTable1->getPhysicalAddr(), false, true};
        pTaskPageDirectory->changePDE(taskFirstPdeIndex, taskPde);

        // Create a page table for the second 4MB of the 8MB region
        currentPage += sizeof(PageTable);
        PageTable* taskPageTable2 = new((unsigned char*)currentPage) PageTable();
        // All of the pages are for the user space
        for(int i=0; i<1024; i++){
            PTE pte = pCpuCore->currentPagingStructure.getPTE((taskRegionPageIndex+(i+1024))/1024, (taskRegionPageIndex+(i+1024)) % 1024);
            PTE taskPTE = {pte.pagePhysicalAddr, false, true};
            taskPageTable2->changePTE(i, taskPTE);
        }
        taskPde = {taskPageTable2->getPhysicalAddr(), false, true};
        pTaskPageDirectory->changePDE(taskSecondPdeIndex, taskPde);

        currentPage += sizeof(PageTable);
        currentPage += KERNEL_STACK_SIZE;
        currentPage += USER_STACK_SIZE;
    }
}

CpuCore::UserTask::~UserTask(){
    if(isRunning){
        // Remove the task from the active task list
        DoublyLinkedListElement<TaskDescriptor>* thisTask = &pCpuCore->taskDescriptorListElements[taskID];

        if(thisTask->prev!=nullptr){
            atomicStore((unsigned int*)&thisTask->prev->next, (unsigned int)thisTask->next);
        }
        else{
            atomicStore((unsigned int*)&pCpuCore->activeTasksListHead, (unsigned int)thisTask->next);
        }
        if(thisTask->next!=nullptr){
            thisTask->next->prev = thisTask->prev;
        }

        thisTask->next = pCpuCore->unusedtaskDescriptorElementsListHead;
        pCpuCore->unusedtaskDescriptorElementsListHead = thisTask;
    }

    if(taskID != -1){
        // Close all sockets that are open for this task
        pCpuCore->pSocketManager->closeAllSocketsForTask((unsigned char)taskID);
    }

    if(pTask8MBRegion!=0){
        // Free allocated memory
        pCpuCore->pKernelPageAlloctor->freeContiguousPages(pTask8MBRegion);
    }
}

SetToRunningStateResponse CpuCore::UserTask::setToRunningState(){
    if(isRunning) return SetToRunningStateResponse::TASK_ALREADY_RUNNING;

    if(pTask8MBRegion==0) return SetToRunningStateResponse::TASK_NOT_PROPERLY_INITIALIZED;

    DoublyLinkedListElement<TaskDescriptor>* newTask = pCpuCore->unusedtaskDescriptorElementsListHead;

    if(newTask==nullptr) return SetToRunningStateResponse::TOO_MANY_TASKS;
    pCpuCore->unusedtaskDescriptorElementsListHead = newTask->next;

    newTask->value.pTask = this;
    // taskList[newTaskId].taskEsp must be the virtual address
    newTask->value.taskEsp = (updatedKernelStack-pTask8MBRegion)+taskSpaceBeginVirtualAddr;
    // taskList[newTaskId].kernelEspStackBegin must be the virtual address
    newTask->value.kernelEspStackBegin = taskSpaceBeginVirtualAddr+USER_TASK_KERNEL_STACK_OFFSET;
    newTask->value.pageDirectoryPhysicalAddr = pTaskPageDirectory->getPhysicalAddr();

    taskID = (newTask-pCpuCore->taskDescriptorListElements);
    isRunning = true;

    newTask->next = pCpuCore->activeTasksListHead;
    newTask->prev = nullptr;
    if(pCpuCore->activeTasksListHead!=nullptr){
        pCpuCore->activeTasksListHead->prev = newTask;
    }
    // Need to be carefull the task list doesn't get ruined while a task switch occurs!
    atomicStore((unsigned int*)&pCpuCore->activeTasksListHead, (unsigned int)newTask);

    return SetToRunningStateResponse::SUCCESS;
}

bool CpuCore::UserTask::isKernelTask(){
    return false;
}

Pair<bool, unsigned int> CpuCore::UserTask::convertContigUserAddrBlockToContigKernelAddrBlock(unsigned int userAddr, unsigned int numBytes){
    if(userAddr<taskSpaceBeginVirtualAddr+USER_TASK_KERNEL_STACK_OFFSET+4 || userAddr+numBytes>taskSpaceBeginVirtualAddr+0x800000){
        return {false, 0};
    }

    return {true, userAddr+(pTask8MBRegion-taskSpaceBeginVirtualAddr)};
}

bool CpuCore::UserTask::addrSpaceIsUserAccessible(unsigned int userAddr, unsigned int numBytes){
    if(userAddr<taskSpaceBeginVirtualAddr+USER_TASK_KERNEL_STACK_OFFSET+4 || userAddr+numBytes>taskSpaceBeginVirtualAddr+0x800000){
        return false;
    }

    return true;
}

void CpuCore::UserTask::setTaskArguments(TaskArguments& taskArgs){
    if(taskArgs.numArgs > MAX_TASK_ARGS) return;

    if(pTask8MBRegion==0) return;

    unsigned int userEspStackBegin = pTask8MBRegion+USER_TASK_USER_STACK_OFFSET;
    for(unsigned int i=0; i<taskArgs.numArgs; i++){
        *((unsigned int*)userEspStackBegin) = taskArgs.args[taskArgs.numArgs-1-i];
        userEspStackBegin -= 4;
    }
    *((unsigned int*)userEspStackBegin) = 0;
    unsigned int virtualUserEspStackBegin = (userEspStackBegin-pTask8MBRegion)+taskSpaceBeginVirtualAddr;
    unsigned int virtualProcessEntry = taskSpaceBeginVirtualAddr+0x4000+KERNEL_STACK_SIZE+USER_STACK_SIZE;

    updatedKernelStack = pCpuCore->interruptHandlerManager.buildIntHandlerStackForUserPriv(InterruptType::PITTimer, pTask8MBRegion+USER_TASK_KERNEL_STACK_OFFSET, virtualUserEspStackBegin, virtualProcessEntry);
}

bool CpuCore::UserTask::setData(unsigned int userAddr, unsigned char* data, unsigned int numBytes){
    if(userAddr<taskSpaceBeginVirtualAddr+USER_TASK_PROCESS_ENTRY_OFFSET || userAddr+numBytes>taskSpaceBeginVirtualAddr+0x800000){
        return false;
    }

    if(pTask8MBRegion==0) return false;

    memCopy((unsigned char*)data, (unsigned char*)(userAddr+(pTask8MBRegion-taskSpaceBeginVirtualAddr)), numBytes);
    return true;
}

CpuCore::KernelTask::KernelTask(CpuCore* pCpuCore)
    :
    Task(pCpuCore)
{
    kernelStackSpaceBegin = 0;
    if(!pCpuCore->isRunning || pCpuCore->currentPagingStructure.getPageDirectoryPhysicalAddr()==pCpuCore->kernelPageDirectoryPhysicalAddr){
        kernelStackSpaceBegin = pCpuCore->pKernelPageAlloctor->allocateContiguousPages(KERNEL_STACK_SIZE/0x1000);
    }
}

CpuCore::KernelTask::~KernelTask(){
    if(isRunning){
        // Remove the task from the active task list
        DoublyLinkedListElement<TaskDescriptor>* thisTask = &pCpuCore->taskDescriptorListElements[taskID];

        if(thisTask->prev!=nullptr){
            atomicStore((unsigned int*)&thisTask->prev->next, (unsigned int)thisTask->next);
        }
        else{
            atomicStore((unsigned int*)&pCpuCore->activeTasksListHead, (unsigned int)thisTask->next);
        }
        if(thisTask->next!=nullptr){
            thisTask->next->prev = thisTask->prev;
        }

        thisTask->next = pCpuCore->unusedtaskDescriptorElementsListHead;
        pCpuCore->unusedtaskDescriptorElementsListHead = thisTask;
    }

    if(taskID != -1){
        // Close all sockets that are open for this task
        pCpuCore->pSocketManager->closeAllSocketsForTask((unsigned char)taskID);
    }

    if(kernelStackSpaceBegin!=0){
        // Free allocated memory
        pCpuCore->pKernelPageAlloctor->freeContiguousPages(kernelStackSpaceBegin);
    }
}

SetToRunningStateResponse CpuCore::KernelTask::setToRunningState(){
    if(isRunning) return SetToRunningStateResponse::TASK_ALREADY_RUNNING;

    if(kernelStackSpaceBegin==0) return SetToRunningStateResponse::TASK_NOT_PROPERLY_INITIALIZED;

    DoublyLinkedListElement<TaskDescriptor>* newTask = pCpuCore->unusedtaskDescriptorElementsListHead;

    if(newTask==nullptr) return SetToRunningStateResponse::TOO_MANY_TASKS;
    pCpuCore->unusedtaskDescriptorElementsListHead = newTask->next;

    newTask->value.pTask = this;
    // For kernel tasks, taskEsp virtual addr is the same as the kernel virtual addr
    newTask->value.taskEsp = updatedKernelStack;
    // for kernel tasks, kernelEspStackBegin (used by the tss) should not actually be used
    newTask->value.kernelEspStackBegin = kernelStackSpaceBegin+KERNEL_STACK_SIZE-4;
    newTask->value.pageDirectoryPhysicalAddr = pCpuCore->currentPagingStructure.getPageDirectoryPhysicalAddr();

    taskID = (newTask-pCpuCore->taskDescriptorListElements);
    isRunning = true;
    
    newTask->next = pCpuCore->activeTasksListHead;
    newTask->prev = nullptr;
    if(pCpuCore->activeTasksListHead!=nullptr){
        pCpuCore->activeTasksListHead->prev = newTask;
    }
    // Need to be carefull the task list doesn't get ruined while a task switch occurs!
    atomicStore((unsigned int*)&pCpuCore->activeTasksListHead, (unsigned int)newTask);

    return SetToRunningStateResponse::SUCCESS;
}

bool CpuCore::KernelTask::isKernelTask(){
    return true;
}

void CpuCore::KernelTask::setProcessCode(unsigned int* processCode, TaskArguments& taskArgs){
    if(taskArgs.numArgs > MAX_TASK_ARGS) return;

    if(kernelStackSpaceBegin==0) return;

    unsigned int kernelEspStackBegin = kernelStackSpaceBegin+KERNEL_STACK_SIZE-4;
    for(unsigned int i=0; i<taskArgs.numArgs; i++){
        *((unsigned int*)kernelEspStackBegin) = taskArgs.args[taskArgs.numArgs-1-i];
        kernelEspStackBegin -= 4;
    }
    *((unsigned int*)kernelEspStackBegin) = 0;
    updatedKernelStack = pCpuCore->interruptHandlerManager.buildIntHandlerStackForKernelPriv(InterruptType::PITTimer, kernelEspStackBegin, (unsigned int)processCode);
}

CpuCore::TimerCallback::TimerCallback(CpuCore* pCpuCore)
    :
    pCpuCore(pCpuCore)
{}

void CpuCore::TimerCallback::run(){
    pCpuCore->timerCounter++;

    yield();
}