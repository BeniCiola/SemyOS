void shell_command_executer(Command command, uint8_t args_number, char *args){
	uint8_t invalid_num_args[] = "Error [args]: Invalid number of arguments";

	if(args != NULL){
		if(args_number != command.args_num){
			print_posy += 16;
			print_posx = 0;
			print_str(invalid_num_args, 255, 50, 50, 255);

			kfree(args);

			return;
		}

		command.function(args);
		kfree(args);
	} else if(command.args_num == 0){
			command.function(NULL);
			kfree(args);
	} else {
		print_posy += 16;
		print_posx = 0;
		print_str(invalid_num_args, 255, 50, 50, 255);

		kfree(args);
	}
}

void shell_command_searcher(char *buffer){
	uint8_t commandnt[] = "Error [command]: The command doesn't exist";

	uint8_t list_i = 0;

	strupper(buffer);

	while(commands_list[list_i].name != NULL){
		uint8_t buffer_letter_i = 0;
		uint8_t name_letter_i = 0;

		while(*(buffer + buffer_letter_i) == ' '){
			buffer_letter_i++;
		}

		while(1){
			if(commands_list[list_i].name[name_letter_i] == 0x00 && (*(buffer + buffer_letter_i) == 0x00 || *(buffer + buffer_letter_i) == ' ')){
				if(*(buffer + buffer_letter_i) == ' '){
					while(*(buffer + buffer_letter_i) == ' '){
						if(*(buffer + buffer_letter_i + 1) == 0x00){
							break;
						}

						buffer_letter_i++;
					}

					if(*(buffer + buffer_letter_i) != ' '){
						char *args = kmalloc(512);
						if(args == NULL) return;

						uint16_t args_i = 0;
						uint8_t args_num = 1;

						while(*(buffer + buffer_letter_i) != 0x00){
							if(*(buffer + buffer_letter_i) == ' ' && *(buffer + buffer_letter_i + 1) != ' ' && *(buffer + buffer_letter_i + 1) != 0x00){
								args_num++;
							}

							args[args_i] = *(buffer + buffer_letter_i);

							args_i++;
							buffer_letter_i++;
						}

						args[args_i] = 0x00;

						shell_command_executer(commands_list[list_i], args_num, args);

						return;
					}
				}
				shell_command_executer(commands_list[list_i], 0, NULL);
				return;
			}

			if(*(buffer + buffer_letter_i) != commands_list[list_i].name[name_letter_i]) break;

			buffer_letter_i++;
			name_letter_i++;
		}

		list_i++;
	}
	print_posx = 0;
	print_posy += 16;
	print_str(commandnt, 255, 50, 50, 255);

	return;
}


void shell(void){
	char *buffer = kmalloc(512);
	if(buffer == NULL) return;

	uint16_t buffer_index = 0;
	bool special_char = false;

	for(uint16_t i = 0; i < 512; i++){
		buffer[i] = 0x00;
	}

	print_str(current_path, 144, 213, 255, 255);
	put_char('>', 255, 255, 255, 255);
	print_posx += 8;
	put_char('_', 255, 255, 255, 255);

	while(1){
		__asm__ volatile ("cli");

		if(kbd_ring_buffer_is_empty(&kbd_buffer)){
			__asm__ volatile ("sti; hlt" ::: "memory");
			continue;
		}

		__asm__ volatile ("sti");

		uint8_t l = kbd_ring_buffer_pop(&kbd_buffer);

		put_char('_', 0, 0, 0, 255);

		if(l == '\n'){
			shell_command_searcher(buffer);

			for(uint16_t i = 0; i < 512; i++){
				buffer[i] = 0x00;
			}

			buffer_index = 0;

			print_posx = 0;
			print_posy += 16;

			print_str(current_path, 144, 213, 255, 255);
			put_char('>', 255, 255, 255, 255);
			print_posx += 8;

			special_char = true;
		}

		if(l == '\b'){
			if(buffer_index > 0){
				if(print_posx == 0){
					print_posx = 1016;
					print_posy-= 16;
				} else {
					print_posx -= 8;
				}

				buffer_index--;

				*(buffer + buffer_index) = 0x00;

				clear_char(0, 0, 0, 255);
				print_posx += 8;
			}
			special_char = true;
		}

		if(!special_char && buffer_index < 511){
			*(buffer + buffer_index) = l;

			buffer_index++;

			put_char(l, 255, 255, 255, 255);

			print_posx += 8;

			if(print_posx >= 1024){
				print_posy += 16;
				print_posx = 0;
			}
		}
		put_char('_', 255, 255, 255, 255);

		special_char = false;
	}
}
