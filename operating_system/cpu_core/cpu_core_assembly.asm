[extern interruptHandlerReturn]
[extern yieldTaskSwitch]

global yieldTaskSwitchIntHandler

;void yieldTaskSwitchIntHandler(unsigned int interruptParam, unsigned int eax)
yieldTaskSwitchIntHandler:
    ;turns out the interrupt manager doesn't really assume any register values are unchanged when calling the interruptHandler
    ;-> no need for pushing and popping all registers
    mov eax, esp

    mov esp, dword [eax+4]
    add esp, 4*999          ;esp should now point to CpuCore::taskSwitchStack[999]

    push eax
    mov edx, esp
    push edx                ;unsigned int* pEsp
    push dword [eax+4]      ;unsigned int interruptParam

    call yieldTaskSwitch    ;void changeEsp(unsigned int interruptParam, unsigned int* pEsp)
    
    add esp, 8
    pop eax

    mov esp, eax
    
    ret