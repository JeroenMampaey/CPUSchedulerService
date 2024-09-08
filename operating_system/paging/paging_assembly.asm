global getCr0

getCr0:
    push eax
    push ebx
    mov eax, [esp+12]
    mov ebx, cr0
    mov [eax], ebx
    pop ebx
    pop eax
    ret