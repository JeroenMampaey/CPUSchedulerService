[bits 32]
[extern main]

KernelEntry:
    call main
    jmp $