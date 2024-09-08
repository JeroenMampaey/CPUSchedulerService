[bits 16]

DAPACK:
            db	0x10
            db	0
    blkcnt:	dw	0		; int 13 resets this to # of blocks actually read/written
    db_add:	dw	0x0     ; memory buffer destination address (0:7c00)
    db_add1: dw	0		; in memory page zero
    d_lba:	dd	0		; put the lba to read in this spot
            dd	0		; more storage bytes only for big lba's ( > 4 bytes )

LoadKernel:
    mov word [blkcnt], 120  ;why 120? because 0x1000+512*120=0x10000, it is impossible to bypass i*0x10000 boundaries while loading with int 13
    mov word [db_add], 0x1000
    mov word [db_add1], 0x0
    mov dword [d_lba], 1
    call Int13

    mov word [blkcnt], 128  ;why 128? because 0x10000+512*128=0x20000, it is impossible to bypass i*0x10000 boundaries while loading with int 13
    mov word [db_add], 0x0
    mov word [db_add1], 0x1000
    mov dword [d_lba], 121
    call Int13

    ret

Int13:
    mov bx, word [blkcnt]
    mov si, DAPACK
	mov ah, 0x42
	mov dl, [BOOT_DRIVE]
	int 0x13
	jc DiskError

    cmp word [blkcnt], bx
    jne DiskError

    ret

DiskError:
    jmp $