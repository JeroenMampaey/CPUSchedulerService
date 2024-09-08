global interruptHandlerReturn

call_handler:
    pusha
    mov bx, ds
    push ebx
    mov bx, (2 * 8)     ; GdtKernelData
    mov ds, bx
    mov es, bx
    mov gs, bx
    mov bx, (6 * 8)     ; INTERRUPT_HANDLERS
    mov fs, bx

    push eax
    
    mov eax, [esp+40]
    mov ecx, 8
    mul ecx

    push dword [fs:eax] ; interrupt handler param
    add eax, 4
    call dword [fs:eax] ; interrupt handler
interruptHandlerReturn:
    add esp, 4

    mov eax, [esp+40]
    cmp eax, 32
    jl l_32_or_ge_48_jump

ge_32_jump:
    cmp eax, 48
    jge l_32_or_ge_48_jump

ge_32_and_l_48_jump:    
    cmp eax, 40
    jl ge_32_and_l_40_jump
    cmp eax, 47
    jne ge_40_and_l_47_jump
    
    ; IRQ 15, check if spurious
    in al, 0xA0         ; Read the In-Service Register (ISR) from the slave PIC
    test al, 0x80       ; Check if the 7th bit is set (ISR for IRQ 15)
    jz l_32_or_ge_48_jump  ; If not set, it's not a valid interrupt, don't send EOI's

ge_40_and_l_47_jump:
    mov al, 0x20
    mov dx, 0xA0
    out dx, al

ge_32_and_l_40_jump:
    cmp eax, 39
    jne ge_32_and_l_39_jump

    ; IRQ 7, check if spurious
    in al, 0x20         ; Read the In-Service Register (ISR) from the master PIC
    test al, 0x80       ; Check if the 7th bit is set (ISR for IRQ 7)
    jz l_32_or_ge_48_jump  ; If not set, it's not a valid interrupt, don't send EOI's

ge_32_and_l_39_jump:
    mov al, 0x20
    mov dx, 0x20
    out dx, al

l_32_or_ge_48_jump:
    pop eax

    pop ebx
    mov ds, bx
    mov es, bx
    mov gs, bx

    popa
    add esp, 8
    iret

global exc0
global exc1
global exc2
global exc3
global exc4
global exc5
global exc6
global exc7
global exc8
global exc9
global exc10
global exc11
global exc12
global exc13
global exc14
global exc15
global exc16
global exc17
global exc18
global exc19
global exc20
global exc21
global exc22
global exc23
global exc24
global exc25
global exc26
global exc27
global exc28
global exc29
global exc30
global exc31

global irq0
global irq1
global irq2
global irq3
global irq4
global irq5
global irq6
global irq7
global irq8
global irq9
global irq10
global irq11
global irq12
global irq13
global irq14
global irq15

global custom0
global custom1
global custom2
global custom3
global custom4
global custom5
global custom6
global custom7

global custom32

; 0: Divide By Zero Exception
exc0:
    cli
    push byte 0
    push byte 0
    jmp call_handler

; 1: Debug Exception
exc1:
    cli
    push byte 0
    push byte 1
    jmp call_handler

; 2: Non Maskable Interrupt Exception
exc2:
    cli
    push byte 0
    push byte 2
    jmp call_handler

; 3: Int 3 Exception
exc3:
    cli
    push byte 0
    push byte 3
    jmp call_handler

; 4: INTO Exception
exc4:
    cli
    push byte 0
    push byte 4
    jmp call_handler

; 5: Out of Bounds Exception
exc5:
    cli
    push byte 0
    push byte 5
    jmp call_handler

; 6: Invalid Opcode Exception
exc6:
    cli
    push byte 0
    push byte 6
    jmp call_handler

; 7: Coprocessor Not Available Exception
exc7:
    cli
    push byte 0
    push byte 7
    jmp call_handler

; 8: Double Fault Exception (With Error Code!)
exc8:
    cli
    push byte 8
    jmp call_handler

; 9: Coprocessor Segment Overrun Exception
exc9:
    cli
    push byte 0
    push byte 9
    jmp call_handler

; 10: Bad TSS Exception (With Error Code!)
exc10:
    cli
    push byte 10
    jmp call_handler

; 11: Segment Not Present Exception (With Error Code!)
exc11:
    cli
    push byte 11
    jmp call_handler

; 12: Stack Fault Exception (With Error Code!)
exc12:
    cli
    push byte 12
    jmp call_handler

; 13: General Protection Fault Exception (With Error Code!)
exc13:
    cli
    push byte 13
    jmp call_handler

; 14: Page Fault Exception (With Error Code!)
exc14:
    cli
    push byte 14
    jmp call_handler

; 15: Reserved Exception
exc15:
    cli
    push byte 0
    push byte 15
    jmp call_handler

; 16: Floating Point Exception
exc16:
    cli
    push byte 0
    push byte 16
    jmp call_handler

; 17: Alignment Check Exception
exc17:
    cli
    push byte 0
    push byte 17
    jmp call_handler

; 18: Machine Check Exception
exc18:
    cli
    push byte 0
    push byte 18
    jmp call_handler

; 19: Reserved
exc19:
    cli
    push byte 0
    push byte 19
    jmp call_handler

; 20: Reserved
exc20:
    cli
    push byte 0
    push byte 20
    jmp call_handler

; 21: Reserved
exc21:
    cli
    push byte 0
    push byte 21
    jmp call_handler

; 22: Reserved
exc22:
    cli
    push byte 0
    push byte 22
    jmp call_handler

; 23: Reserved
exc23:
    cli
    push byte 0
    push byte 23
    jmp call_handler

; 24: Reserved
exc24:
    cli
    push byte 0
    push byte 24
    jmp call_handler

; 25: Reserved
exc25:
    cli
    push byte 0
    push byte 25
    jmp call_handler

; 26: Reserved
exc26:
    cli
    push byte 0
    push byte 26
    jmp call_handler

; 27: Reserved
exc27:
    cli
    push byte 0
    push byte 27
    jmp call_handler

; 28: Reserved
exc28:
    cli
    push byte 0
    push byte 28
    jmp call_handler

; 29: Reserved
exc29:
    cli
    push byte 0
    push byte 29
    jmp call_handler

; 30: Reserved
exc30:
    cli
    push byte 0
    push byte 30
    jmp call_handler

; 31: Reserved
exc31:
    cli
    push byte 0    ;err_code
    push byte 31   ;int_no
    jmp call_handler

; IRQ handlers
irq0:
	cli
	push byte 0
	push byte 32
	jmp call_handler

irq1:
	cli
	push byte 1
	push byte 33
	jmp call_handler

irq2:
	cli
	push byte 2
	push byte 34
	jmp call_handler

irq3:
	cli
	push byte 3
	push byte 35
	jmp call_handler

irq4:
	cli
	push byte 4
	push byte 36
	jmp call_handler

irq5:
	cli
	push byte 5
	push byte 37
	jmp call_handler

irq6:
	cli
	push byte 6
	push byte 38
	jmp call_handler

irq7:
	cli
	push byte 7
	push byte 39
	jmp call_handler

irq8:
	cli
	push byte 8
	push byte 40
	jmp call_handler

irq9:
	cli
	push byte 9
	push byte 41
	jmp call_handler

irq10:
	cli
	push byte 10
	push byte 42
	jmp call_handler

irq11:
	cli
	push byte 11
	push byte 43
	jmp call_handler

irq12:
	cli
	push byte 12
	push byte 44
	jmp call_handler

irq13:
	cli
	push byte 13
	push byte 45
	jmp call_handler

irq14:
	cli
	push byte 14
	push byte 46
	jmp call_handler

irq15:
	cli
	push byte 15
	push byte 47
	jmp call_handler

custom0:
    cli
    push byte 0
    push byte 48
    jmp call_handler

custom1:
    cli
    push byte 0
    push byte 49
    jmp call_handler

custom2:
    cli
    push byte 0
    push byte 50
    jmp call_handler

custom3:
    cli
    push byte 0
    push byte 51
    jmp call_handler

custom4:
    cli
    push byte 0
    push byte 52
    jmp call_handler

custom5:
    cli
    push byte 0
    push byte 53
    jmp call_handler

custom6:
    cli
    push byte 0
    push byte 54
    jmp call_handler

custom7:
    cli
    push byte 0
    push byte 55
    jmp call_handler

custom32:
    cli
    push byte 0
    push byte 80
    jmp call_handler

global createIntStackForUserPriv
global createIntStackForKernelPriv

;void createIntStackForUserPriv(unsigned int* pTopKernelStack, unsigned int kernelStack, unsigned int userStack, unsigned int processEntry, unsigned int intNumber)
;creates a stack at kernelStack as if ring 3 code was interrupted
;pTopKernelStack is used as a way to pass the return value
createIntStackForUserPriv:
    push eax            ;push every used register on the stack
    push ebx
    push ecx
    push edx

    mov eax, [esp+28]   ;userStack
    mov ebx, [esp+32]   ;processEntry
    mov ecx, [esp+24]   ;kernelStack

    mov edx, esp
    mov esp, ecx        ;from this point, current stack is the kernel stack, edx stores the old esp

    push (4 * 8) | 3    ;ss
    push eax            ;useresp
    pushf               ;eflags
    push (3 * 8) | 3    ;cs
    push ebx            ;eip
    push byte 0         ;err_code for irq0
    mov eax, [edx+36]   ;intNumber
	push eax            ;push intNumber

    pusha               ;eax, ecx, edx, ebx, esp, ebp, esi, edi
                        ;this however is less obvious then it seems:
                        ;   This function is primarily used to create a initial stack for new tasks, this stack will be used to exit the timer interrupt handler
                        ;   The esp pushed with pusha here however is incorrect! because these tasks will use paging to change the esp value to some other virtual address
                        ;   But luckily: https://www.felixcloutier.com/x86/popa:popad, popa (used in the call handler function defined above) ignores the esp value!
    
    push (4 * 8) | 3    ;ds
    push 0x0            ;eax register, not really important first time
    push 0x0            ;interruptHandlerParam, not really important first time
    push interruptHandlerReturn ;return back to call_handler

    mov eax, dword [edx+20]   ;pTopKernelStack
    mov dword [eax], esp      ;*pTopKernelStack = newKernelEsp

    mov esp, edx        ;esp is back normal again

    pop edx             ;pop every used register from the stack
    pop ecx
    pop ebx
    pop eax
    ret

;void createIntStackForKernelPriv(unsigned int* pTopKernelStack, unsigned int kernelStack, unsigned int processEntry, unsigned int intNumber)
;creates a stack at kernelStack as if ring 0 code was interrupted
;pTopKernelStack is used as a way to pass the return value
createIntStackForKernelPriv:
    push eax            ;push every used register on the stack
    push ebx
    push ecx
    push edx

    mov ebx, [esp+28]   ;processEntry
    mov ecx, [esp+24]   ;kernelStack

    mov edx, esp
    mov esp, ecx        ;from this point, current stack is the kernel stack, edx stores the old esp

    pushf               ;eflags
    push (1 * 8)        ;cs
    push ebx            ;eip
    push byte 0         ;err_code for irq0
    mov eax, [edx+32]   ;intNumber
	push eax            ;push intNumber

    pusha               ;eax, ecx, edx, ebx, esp, ebp, esi, edi
                        ;this however is less obvious then it seems:
                        ;   This function is primarily used to create a initial stack for new tasks, this stack will be used to exit the timer interrupt handler
                        ;   The esp pushed with pusha here however is incorrect! because these tasks will use paging to change the esp value to some other virtual address
                        ;   But luckily: https://www.felixcloutier.com/x86/popa:popad, popa (used in the call handler function defined above) ignores the esp value!
    
    push (2 * 8)        ;ds
    push 0x0            ;eax register, not really important first time
    push 0x0            ;interruptHandlerParam, not really important first time
    push interruptHandlerReturn ;return back to call_handler

    mov eax, dword [edx+20]   ;pTopKernelStack
    mov dword [eax], esp      ;*pTopKernelStack = newKernelEsp

    mov esp, edx        ;esp is back normal again

    pop edx             ;pop every used register from the stack
    pop ecx
    pop ebx
    pop eax
    ret