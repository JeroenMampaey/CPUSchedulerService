GdtStart:

GdtNull:                    ;8 byte null descriptor
    dd 0x0
    dd 0x0

GdtKernelCode:
    dw 0xffff               ;bits 0->15 are from the limit
    dw 0x0                  ;bits 16->31 are from the base address, 0 in this case since we are not interested in using segmentation tricks
    db 0x0                  ;bits 32->39 continues the base address
    db 10011010b            ;bits 40->47 is the access byte: Present, Highest Privilege, Code or Data Segment, Executable, Read and Write
    db 11001111b            ;bits 48->51 continues the limit (thus total limit is 0xFFFFF), bits 52->55 are flags: Protected Mode Segment, Limit is Page Size Granularity (thus limit is 4GB)
    db 0x0                  ;bits 56->63 continue the base address

GdtKernelData:
    dw 0xffff               ;bits 0->15 are from the limit
    dw 0x0                  ;bits 16->31 are from the base address, 0 in this case since we are not interested in using segmentation tricks
    db 0x0                  ;bits 32->39 continues the base address
    db 10010010b            ;bits 40->47 is the access byte: Present, Highest Privilege, Code or Data Segment, Not Executable, Read and Write
    db 11001111b            ;bits 48->51 continues the limit (thus total limit is 0xFFFFF), bits 52->55 are flags: Protected Mode Segment, Limit is Page Size Granularity (thus limit is 4GB)
    db 0x0                  ;bits 56->63 continue the base address

GdtUserCode:
    dw 0xffff               ;bits 0->15 are from the limit
    dw 0x0                  ;bits 16->31 are from the base address, 0 in this case since we are not interested in using segmentation tricks
    db 0x0                  ;bits 32->39 continues the base address
    db 11111010b            ;bits 40->47 is the access byte: Present, User Privilege, Code or Data Segment, Executable, Read and Write
    db 11001111b            ;bits 48->51 continues the limit (thus total limit is 0xFFFFF), bits 52->55 are flags: Protected Mode Segment, Limit is Page Size Granularity (thus limit is 4GB)
    db 0x0                  ;bits 56->63 continue the base address

GdtUserData:
    dw 0xffff               ;bits 0->15 are from the limit
    dw 0x0                  ;bits 16->31 are from the base address, 0 in this case since we are not interested in using segmentation tricks
    db 0x0                  ;bits 32->39 continues the base address
    db 11110010b            ;bits 40->47 is the access byte: Present, User Privilege, Code or Data Segment, Not Executable, Read and Write
    db 11001111b            ;bits 48->51 continues the limit (thus total limit is 0xFFFFF), bits 52->55 are flags: Protected Mode Segment, Limit is Page Size Granularity (thus limit is 4GB)
    db 0x0                  ;bits 56->63 continue the base address

TSS:
    dw 0x0                  ;bits 0->15 are from the limit, filled in later
    dw 0x0                  ;bits 16->31 are from the base address, filled in later
    db 0x0                  ;bits 32->39 continues the base address, filled in later
    db 10001001b            ;bits 40->47 is the access byte: TSS (Available), State Segment, User Privilege, Present
    db 00000000b            ;bits 48->51 continues the limit, filled in later, bits 52->55 are flags: Protected Mode Segment, 16-bit Protected Mode Segment (?), Limit is Byte Granularity
    db 0x0                  ;bits 56->63 continue the base address, filled in later

INTERRUPT_HANDLERS:
    dw 0xffff               ;bits 0->15 are from the limit
    dw 0x0                  ;bits 16->31 are from the base address, 0 in this case since we are not interested in using segmentation tricks
    db 0x0                  ;bits 32->39 continues the base address
    db 10010010b            ;bits 40->47 is the access byte: Present, Highest Privilege, Code or Data Segment, Not Executable, Read and Write
    db 11001111b            ;bits 48->51 continues the limit (thus total limit is 0xFFFFF), bits 52->55 are flags: Protected Mode Segment, Limit is Page Size Granularity (thus limit is 4GB)
    db 0x0                  ;bits 56->63 continue the base address

GdtEnd:

GdtDescriptor:
    dw GdtEnd-GdtStart-1    ;bits 0->15 are the size (size must be -1 as explained here: https://wiki.osdev.org/Global_Descriptor_Table)
    dd GdtStart             ;bits 16->48 are the virtual address of the gdt

CODE_SEG equ GdtKernelCode-GdtStart
DATA_SEG equ GdtKernelData-GdtStart