[bits 16]

SwitchToPm:
    cli
    lgdt [GdtDescriptor]
    
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax
    jmp CODE_SEG:InitPm
    
[bits 32]

NEW_STACK_ADDR equ 0x10FFFC

InitPm:
    mov ax, DATA_SEG
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    mov ebp, NEW_STACK_ADDR
    mov esp, ebp
    
    call BEGIN_PM