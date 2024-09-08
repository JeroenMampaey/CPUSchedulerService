[org 0x600]

BOOT_SECTOR_SIZE equ 0x100
ORIG_BOOT_ADDR equ 0x7C00
NEW_BOOT_ADDR equ 0x0600

[bits 16]

BootStart:
    ;start by moving over the bootsector to address 0x600
    ;this way, the kernel (which will be loaded at 0x1000) will get alot of space
    ;first few lines are from https://wiki.osdev.org/MBR_(x86)
    cli                         ; We do not want to be interrupted
    xor ax, ax                  
    mov ds, ax                  
    mov es, ax                  
    mov ss, ax                  
    mov sp, ax                  
    mov cx, BOOT_SECTOR_SIZE
    mov si, ORIG_BOOT_ADDR
    mov di, NEW_BOOT_ADDR
    rep movsw                   ; Move bootsector to new location
    jmp 0:NewStart              ; Jump to the new location of the bootsector

TEMP_STACK_ADDR equ 0x5FF

NewStart:
    sti
    mov [BOOT_DRIVE], dl        ;BIOS sets dl to drive number, store value so it can be used in LoadKernel
    mov bp, TEMP_STACK_ADDR
    mov sp, bp                  ;setting up a temporary stack used for booting

    call LoadKernel

    call CreateMemTable         ;ask BIOS for a memory map and place it at addres 0x800

    call EnableA20              ;make sure the A20 line is enabled

    call SwitchToPm             ;switch to protected mode

    jmp $

%include "boot/enable_A20.asm"
%include "boot/load_kernel.asm"
%include "boot/gdt_table.asm"
%include "boot/switch_to_pm.asm"
%include "boot/create_mem_table.asm"

[bits 32]

KERNEL_OFFSET equ 0x1000

BEGIN_PM:
    call KERNEL_OFFSET          ;execute kernel code
    
    jmp $

BOOT_DRIVE db 0                 ;drive number

times 510-($-$$) db 0
dw 0xaa55