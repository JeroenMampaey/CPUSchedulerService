global memoryCopyDwords
global memoryCopyBytes
global clearDwords
global clearBytes

;memoryCopyDwords(unsigned char* from, unsigned char* to, int numberOfDwords)
;move "numberOfDwords" dwords from address "from" to address "to"
memoryCopyDwords:
    push ecx
    push edi
    push esi

    mov esi, dword [esp+16]
    mov edi, dword [esp+20]

    mov	ecx, dword [esp+24]
    rep	movsd

    pop esi
    pop edi
    pop ecx
    ret

;memoryCopyBytes(unsigned char* from, unsigned char* to, int numberOfBytes)
;move "numberOfBytes" bytes from address "from" to address "to"
memoryCopyBytes:
    push ecx
    push edi
    push esi

    mov esi, dword [esp+16]
    mov edi, dword [esp+20]

    mov	ecx, dword [esp+24]
    rep	movsb

    pop esi
    pop edi
    pop ecx
    ret

;clearDwords(unsigned char* addres, int numberOfDwords)
;place zeros for "numberOfDwords" dwords starting at address "address"
clearDwords:
    push ecx
    push edi
    push eax

    mov edi, dword [esp+16]
    mov eax, 0

    mov ecx, dword [esp+20]
    rep stosd

    pop eax
    pop edi
    pop ecx
    ret

;clearBytes(unsigned char* addres, int numberOfBytes)
;place zeros for "numberOfBytes" bytes starting at address "address"
clearBytes:
    push ecx
    push edi
    push eax

    mov edi, dword [esp+16]
    mov eax, 0

    mov ecx, dword [esp+20]
    rep stosb

    pop eax
    pop edi
    pop ecx
    ret