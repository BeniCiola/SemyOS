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

Command commands_list[] = {
	{"clear", false, 0, shell_command_clear},
	{"echo", true, 1, shell_command_echo},
	{NULL, false, 0, NULL}
};
