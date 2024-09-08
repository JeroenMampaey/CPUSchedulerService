global atomicStore
global conditionalExchange

;atomicStore(unsigned int* destination, unsigned int value)
;store value at destination using the lock prefix
atomicStore:
    push eax
    push ebx
    push ecx

    mov ebx, dword [esp+16] ; ebx = destination
    mov ecx, dword [esp+20] ; ecx = value
    mov eax, dword [ebx]    ; eax = *destination

    lock cmpxchg dword [ebx], ecx ; if(eax==*ebx){*ebx=ecx}else{...}
                                  ; thus if(*destination==*destination){*destination=value}
                                  ; thus *destination=value

    pop ecx
    pop ebx
    pop eax
    ret

;conditionalExchange(unsigned int* destination, unsigned int expected, unsigned int desired)
;*destination=desired only if *destination==expected
conditionalExchange:
    push ebx
    push ecx

    mov ebx, dword [esp+12] ; ebx = destination
    mov ecx, dword [esp+20] ; ecx = desired
    mov eax, dword [esp+16] ; eax = expected

    lock cmpxchg dword [ebx], ecx ; if(eax==*ebx){*ebx=ecx}else{changes to flags and eax}
                                  ; thus if(expected==*destination){*destination=desired}else{changes to flags and eax}
    setz al                       ; al=1 if eax==*ebx was successfull
    movzx eax, al
    
    pop ecx
    pop ebx
    ret