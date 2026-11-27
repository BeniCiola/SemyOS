typedef struct {
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
} syscall_regs_t;

void syscall_handler(syscall_regs_t *regs){
	switch(regs->eax){
		case SYS_EXIT:
			task_exit();
			break;

		case SYS_PRINT:
			print_str((uint8_t *)regs->ebx, 255, 255, 255, 255);
			break;

		case SYS_SETCURSOR:
			print_posx = regs->ebx;
			print_posy = regs->edx;
			break;

		default:
			break;
	}
}
