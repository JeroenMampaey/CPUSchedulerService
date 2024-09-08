global loadPageDirectory
global enablePaging

global invalidateTLB
global invalidatePage
global getPageDirectory

loadPageDirectory:
    push ebp
    mov ebp, esp
    mov eax, [esp+8]
    mov cr3, eax
    mov esp, ebp
    pop ebp
    ret

enablePaging:
    push ebp
    mov ebp, esp
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax
    mov esp, ebp
    pop ebp
    ret

invalidateTLB:
    push eax
    mov eax, cr3
    mov cr3, eax
    pop eax
    ret

invalidatePage:
    push eax
    mov eax, [esp+8]
    invlpg [eax]
    pop eax
    ret

getPageDirectory:
    push eax
    push ebx
    mov eax, [esp+12]
    mov ebx, cr3
    mov [eax], ebx
    pop ebx
    pop eax
    ret
