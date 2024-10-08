[bits 16]

;copied this routine from https://wiki.osdev.org/Detecting_Memory_(x86)

MEM_TABLE_DEST_HIGH equ 0x0
MEM_TABLE_DEST_LOW equ 0x800

CreateMemTable:
	push es
    mov ax, MEM_TABLE_DEST_HIGH
    mov es, ax
    mov ax, 0
    mov di, MEM_TABLE_DEST_LOW
    add di, 0x04       
	xor ebx, ebx	
	xor bp, bp		
    mov edx, 0x0534D4150
	mov eax, 0xe820
	mov [es:di + 20], dword 1	; force a valid ACPI 3.X entry
	mov ecx, 24		; ask for 24 bytes
	int 0x15
	jc short .failed	; carry set on first call means "unsupported function"
	mov edx, 0x0534D4150	; Some BIOSes apparently trash this register?
    cmp eax, edx		; on success, eax must have been reset to "SMAP"
	jne short .failed
	test ebx, ebx		; ebx = 0 implies list is only 1 entry long (worthless)
	je short .failed
	jmp short .jmpin     ;jmpin
.e820lp:
	mov eax, 0xe820		; eax, ecx get trashed on every int 0x15 call
	mov [es:di + 20], dword 1	; force a valid ACPI 3.X entry
	mov ecx, 24		; ask for 24 bytes again
	int 0x15
	jc short .e820f		; carry set means "end of list already reached"
	mov edx, 0x0534D4150	; repair potentially trashed register
.jmpin:
	jcxz .skipent		; skip any 0 length entries
	cmp cl, 20		; got a 24 byte ACPI 3.X response?
	jbe short .notext
	test byte [es:di + 20], 1	; if so: is the "ignore this data" bit clear?
	je short .skipent
.notext:
	mov ecx, [es:di + 8]	; get lower uint32_t of memory region length
	or ecx, [es:di + 12]	; "or" it with upper uint32_t to test for zero
	jz .skipent		; if length uint64_t is 0, skip entry
	inc bp			; got a good entry: ++count, move to next storage spot
	add di, 24
.skipent:
	test ebx, ebx		; if ebx resets to 0, list is complete
	jne short .e820lp
.e820f:
	mov ax, MEM_TABLE_DEST_HIGH
	mov es, ax
	mov ax, MEM_TABLE_DEST_LOW
	mov di, ax
	mov [es:di], bp	; store the entry count
	clc			; there is "jc" on end of list to this point, so the carry must be cleared
	pop es
	ret
.failed:
	stc			; "function unsupported" error exit
	pop es
	ret