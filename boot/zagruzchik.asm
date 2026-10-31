bits 16
org 0x7C00

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    mov [boot_drive], dl

    ; El que toque esto le meto cadena perpétua
    mov ax, 0xB800
    mov es, ax
    mov byte [es:0], 'C'
    mov byte [es:1], 0x07

    xor ax, ax
    mov es, ax 

    mov si, dap
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc disk_error

	xor ax, ax
	mov ds, ax
	mov es, ax

	mov	ax, 0x4F01
	mov	cx, 0x4118
	mov	di, 0x7E00
	int	0x10
	cmp	ax, 0x004F
	jne	vbe_error

	mov	edx, [0x7E00 + 0x28] 

	mov	ax, 0x4F02
	mov	bx, 0x4118
	int	0x10
	cmp	ax, 0x004F
	jne	vbe_error

    cli
    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp 0x08:init_pm


vbe_error:
	mov ax, 0xB800
	mov es, ax
	mov byte [es:0], 'V'
	mov byte [es:1], 0x04
	jmp hang16


disk_error:
    mov ax, 0xB800
    mov es, ax
    mov byte [es:0], 'D'
    mov byte [es:1], 0x04


hang16:
    cli
    hlt
    jmp hang16

boot_drive db 0

align 4
dap:
    db 0x10
    db 0
    dw 32
    dw 0x8000
    dw 0x0000
    dq 1

align 8
gdt_start:
    dq 0

    dw 0xFFFF, 0x0000
    db 0x00, 10011010b, 11001111b, 0x00

    dw 0xFFFF, 0x0000
    db 0x00, 10010010b, 11001111b, 0x00
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

bits 32
init_pm:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov esp, 0x90000
    mov ebp, esp

	cld
    jmp 0x8000

times 510-($-$$) db 0
dw 0xAA55
