bits 32
global idt_flush

idt_flush:
	mov eax, [esp + 4]

	lidt [eax]
	ret

global timer_handler_asm
global keyboard_handler_asm
global syscall_handler_asm

extern timer_handler
extern keyboard_handler
extern syscall_handler

timer_handler_asm:
	pushfd
	pushad
	call timer_handler
	popad
	popfd
	iretd

keyboard_handler_asm:
	pushfd
	pushad
	call keyboard_handler
	popad
	popfd
	iretd

syscall_handler_asm:
	pushfd
	pushad
	push esp

	call syscall_handler

	pop eax
	popad
	popfd

	iretd
