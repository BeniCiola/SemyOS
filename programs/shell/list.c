typedef void (*CommandFunc)(char *args);

typedef struct {
	char *name;
	bool args;
	uint8_t args_num;
	CommandFunc function;
} Command;


void shell_command_clear(char *args){
	clear_screen(0, 0, 0, 255);
	print_posx = 0;
	print_posy = -16;
}

void shell_command_echo(char *args){
	print_posx = 0;
	print_posy += 16;
	print_str((const uint8_t *)args, 255, 255, 255, 255);
}

void shell_command_reboot(char *args){
	__asm__ __volatile__("cli");

	outb(0x21, 0xFF);
	outb(0xA1, 0xFF);

	outb(0xCF9, 0x02);
	outb(0xCF9, 0x06);

	if ((inb(0x64) & 0x02) == 0) {
		outb(0x64, 0xFE);
	}

	struct {
		unsigned short limite;
		unsigned int base;
	} __attribute__((packed)) empty_idt = {0, 0};

	__asm__ __volatile__("lidt %0" : : "m"(empty_idt));
	__asm__ __volatile__("int $3");

	while(1) { __asm__ __volatile__("hlt"); }
}

Command commands_list[] = {
	{"clear", false, 0, shell_command_clear},
	{"echo", true, 1, shell_command_echo},
	{"reboot", false, 0, shell_command_reboot},
	{NULL, false, 0, NULL}
};
