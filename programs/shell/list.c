#define TOTAL_SHELL_COMMANDS 16

typedef void (*CommandFunc)(char *args);

typedef struct {
	char *name;
	bool args;
	uint8_t args_num;
	CommandFunc function;
	const char *description;
} Command;

extern Command commands_list[TOTAL_SHELL_COMMANDS];


// COMMAND HELP--------------------------------------------
void shell_command_help(char *args){
	(void)args;

	for(uint16_t i = 0; commands_list[i].name != NULL; i++){
		crlf_cursor();
		print_str((uint8_t *)"- ", 255, 255, 255, 255);
		print_str((uint8_t *)commands_list[i].name, 255, 255, 255, 255);
		print_str((uint8_t *)" - ", 255, 255, 255, 255);
		print_str((uint8_t *)commands_list[i].description, 255, 255, 144, 255);
	}
}


// COMMAND CLEAR-------------------------------------------
void shell_command_clear(char *args){
	(void)args;

	clear_screen(0, 0, 0, 255);
	print_posx = 0;
	print_posy = -16;
}


// COMMAND ECHO--------------------------------------------
void shell_command_echo(char *args){
	crlf_cursor();
	print_str((const uint8_t *)args, 255, 255, 255, 255);
}


// COMMAND REBOOT------------------------------------------
void shell_command_reboot(char *args){
	(void)args;

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


// COMMAND AUDIOTEST-----------------------------------
void shell_command_audiotest(char *args){
	(void)args;

	hda_play_tone(660, 1000);
}


// COMMAND SLAVA---------------------------------------
void shell_command_slava(char *args){
	(void)args;

	note_t ussr_anthem[] = {
	    {196, 320},
	    {262, 550},
	    {196, 430},
	    {220, 200},
	    {247, 550},
	    {165, 320},
	    {165, 320},
	    {220, 550},
	    {196, 430},
	    {175, 210},
	    {196, 550},
	    {131, 320},
	    {131, 320},

	    {147, 550},
	    {147, 320},
	    {165, 320},
	    {175, 550},
	    {175, 320},
	    {196, 320},
	    {220, 550},
	    {247, 380},
	    {262, 250},
	    {294, 750},

	    {196, 320},
	    {330, 550},
	    {294, 430},
	    {262, 210},
	    {294, 550},
	    {247, 320},
	    {196, 320},
	    {262, 550},
	    {247, 430},
	    {220, 240},
	    {247, 550},
	    {165, 310},
	    {165, 320},

	    {220, 550},
	    {196, 430},
	    {175, 240},
	    {196, 550},
	    {131, 290},
	    {131, 320},
	    {262, 550},
	    {247, 430},
	    {220, 190},
	    {196, 750}
	};

	hda_play_notes(ussr_anthem, sizeof(ussr_anthem) / sizeof(note_t));
}


// COMMAND ISHERE------------------------------------------
void shell_command_ishere(char *args){
	if(mik_lookup(current_dir_inode, args) >= 0){
		crlf_cursor();
		print_str((uint8_t *)"Yes, here it is.", 255, 255, 255, 255);
	} else {
		crlf_cursor();
		print_str((uint8_t *)"No, here not it is.", 255, 255, 255, 255);
	}
}


// COMMAND MKDIR-------------------------------------------
void shell_command_mkdir(char *args){
	mik_create(current_dir_inode, args, MIK_TYPE_DIR, MIK_PERM_WRITE | MIK_PERM_READ);
}


// COMMAND TOUCH-------------------------------------------
void shell_command_touch(char *args){
	mik_create(current_dir_inode, args, MIK_TYPE_FILE, MIK_PERM_WRITE | MIK_PERM_READ);
}


// COMMAND RM----------------------------------------------
void shell_command_rm(char *args){
	uint8_t i = 0;

	char type;
	type = args[i];

	if(args[i + 1] != ' ' || (type != 'f' && type != 'd')){
		crlf_cursor();
		print_str((uint8_t *)"Error [invalid type]: Only 'f' and 'd'.", 255, 50, 50, 255);
		return;
	}

	i += 2;

	char name[29];
	uint8_t c = 0; //Uff, C reference.

	for(; args[i] != 0; i++){
		if(c >= 28){
			crlf_cursor();
			print_str((uint8_t *)"Error [size]: The name is too long.", 255, 50, 50, 255);
			return;
		}

		name[c] = args[i];
		c++; //Uff, C++ reference.
	} name[c] = 0;

	bool is_dir = (type == 'd') ? true : false;

	int result = mik_delete(current_dir_inode, name, is_dir);

	switch (result){
		case 1:
			break;
		case -1:
			crlf_cursor();
			print_str((uint8_t *)"No no, you cannot remove the root directory.", 255, 50, 50, 255);
			break;
		case -2:
			crlf_cursor();
			print_str((uint8_t *)"The target doesn't exist.", 255, 50, 50, 255);
			break;
		case -3:
			crlf_cursor();
			print_str((uint8_t *)"The type doesn't match with the reality.", 255, 50, 50, 255);
			break;
		case -4:
			crlf_cursor();
			print_str((uint8_t *)"The directory still has content.", 255, 50, 50, 255);
			break;
		default:
			break;
	}
}


// COMMAND MV----------------------------------------------
void shell_command_mv(char *args){
	char thing_name[29];
	char rout_dest[sizeof(current_path)];

	uint16_t i = 0;

	for(; args[i] != ' '; i++){
		if(i >= 28){
			crlf_cursor();
			print_str((uint8_t *)"Error [size]: The thing name is too long.", 255, 50, 50, 255);
			return;
		}
		thing_name[i] = args[i];
	}
	thing_name[i] = 0;

	int target_thing = mik_lookup(current_dir_inode, thing_name);
	if(target_thing == 0){
		crlf_cursor();
		print_str((uint8_t *)"Error [mv]: Cannot move the root dir.", 255, 50, 50, 255);
		return;
	} else if(target_thing < 0){
		crlf_cursor();
		print_str((uint8_t *)"Error: The thing not exist here.", 255, 50, 50, 255);
		return;
	}

	uint16_t k = 0;
	i++;

	for(; args[i] != 0; i++){
		if(k >= sizeof(current_path) - 1){
			crlf_cursor();
			print_str((uint8_t *)"Error [size]: The rout is too long.", 255, 50, 50, 255);
			return;
		}
		rout_dest[k] = args[i];
		k++;
	}
	rout_dest[k] = 0;

	char target_dir[29];
	uint32_t actual_inode = current_dir_inode;
	i = 0;

	uint32_t page_phys = pmm_alloc_block();
	vmm_map_page(page_phys, page_phys, PTE_PCD | PTE_PRESENT | PTE_RW);
	uint8_t *buf = (uint8_t *)page_phys;

	mik_inode_t inode;

	while(rout_dest[i] != 0){
		k = 0;

		for(; rout_dest[i] != '/' && rout_dest[i] != 0; i++){
			if(k >= 28){
				crlf_cursor();
				print_str((uint8_t *)"Error [Size]: Some dir has a too long name.", 255, 50, 50, 255);
				pmm_free_block(page_phys);
				return;
			}
			target_dir[k] = rout_dest[i];
			k++;
		}
		target_dir[k] = 0;

		if(rout_dest[i] == '/') i++;

		if(k == 0) continue;

		mik_read_inode(actual_inode, &inode, buf);

		if(strcmp(target_dir, "..")){
			actual_inode = inode.parent_inode;
			continue;
		}

		int target = mik_lookup(actual_inode, target_dir);
		if(target < 0){
			crlf_cursor();
			print_str((uint8_t *)"Error: Some dir was not found (or simply is not a dir).", 255, 50, 50, 255);
			pmm_free_block(page_phys);
			return;
		}

		actual_inode = target;
	}

	if(actual_inode == current_dir_inode || actual_inode == (uint32_t)target_thing){
		crlf_cursor();
		print_str((uint8_t *)"Error [mv]: Invalid destination directory.", 255, 50, 50, 255);
		pmm_free_block(page_phys);
		return;
	}

	mik_read_inode(actual_inode, &inode, buf);
	if(inode.type != MIK_TYPE_DIR){
		crlf_cursor();
		print_str((uint8_t *)"Error [type]: The destination is not a dir.", 255, 50, 50, 255);
		pmm_free_block(page_phys);
		return;
	}

	if(mik_lookup(actual_inode, thing_name) != -1){
		crlf_cursor();
		print_str((uint8_t *)"Error [mv]: Already exists in destination.", 255, 50, 50, 255);
		pmm_free_block(page_phys);
		return;
	}

	uint32_t curr_cluster = inode.first_cluster;
	bool inserted = false;

	while(curr_cluster != MIK_CLUSTER_EOF && !inserted){
		mik_read_block(curr_cluster, buf);
		mik_cluster_t *c = (mik_cluster_t *)buf;
		mik_dirent_t *entries = (mik_dirent_t *)c->data;

		for(uint32_t j = 0; j < 127; j++){
			if(entries[j].inode_id == 0){
				entries[j].inode_id = target_thing;
				uint32_t len = strsize(thing_name);
				for(uint32_t m = 0; m < 28; m++){
					entries[j].name[m] = (m < len) ? thing_name[m] : 0;
				}
				mik_write_block(curr_cluster, buf);
				inserted = true;
				break;
			}
		}
		curr_cluster = c->next_cluster;
	}

	if(!inserted){
		crlf_cursor();
		print_str((uint8_t *)"Error [mv]: Destination directory is full.", 255, 50, 50, 255);
		pmm_free_block(page_phys);
		return;
	}

	mik_read_inode(actual_inode, &inode, buf);
	inode.size += sizeof(mik_dirent_t);
	mik_write_inode(actual_inode, &inode, buf);

	mik_read_inode(current_dir_inode, &inode, buf);
	curr_cluster = inode.first_cluster;
	bool removed = false;

	while(curr_cluster != MIK_CLUSTER_EOF && !removed){
		mik_read_block(curr_cluster, buf);
		mik_cluster_t *c = (mik_cluster_t *)buf;
		mik_dirent_t *entries = (mik_dirent_t *)c->data;

		for(uint32_t j = 0; j < 127; j++){
			if(entries[j].inode_id == (uint32_t)target_thing){
				entries[j].inode_id = 0;
				for(uint32_t m = 0; m < 28; m++) entries[j].name[m] = 0;
				mik_write_block(curr_cluster, buf);
				removed = true;
				break;
			}
		}
		curr_cluster = c->next_cluster;
	}

	mik_read_inode(current_dir_inode, &inode, buf);
	inode.size -= sizeof(mik_dirent_t);
	mik_write_inode(current_dir_inode, &inode, buf);

	mik_read_inode(target_thing, &inode, buf);
	inode.parent_inode = actual_inode;
	mik_write_inode(target_thing, &inode, buf);

	pmm_free_block(page_phys);
}


// COMMAND CD----------------------------------------------
void shell_command_cd(char *args){
	uint32_t page_phys = pmm_alloc_block();
	vmm_map_page(page_phys, page_phys, PTE_PCD | PTE_PRESENT | PTE_RW);
	uint8_t *buf = (uint8_t *)page_phys;

	mik_inode_t inode;
	mik_read_inode(current_dir_inode, &inode, buf);

	if(strcmp(args, "..")){
		current_dir_inode = inode.parent_inode;
		pmm_free_block(page_phys);

		uint32_t len = strsize(current_path);
		if(len > 3){
			current_path[len - 2] = 0;
			for(uint32_t i = len - 3; current_path[i] != '/'; i--){
				current_path[i] = 0;
			}
		}

		return;
	}


	int target = mik_lookup(current_dir_inode, args);
	mik_read_inode(target, &inode, buf);

	if(target == -1){
		crlf_cursor();
		print_str((uint8_t *)"Directory not found.", 255, 55, 55, 255);
	} else if(inode.type != MIK_TYPE_DIR){
		crlf_cursor();
		print_str((uint8_t *)"Not a directory.", 255, 55, 55, 255);
	} else {
		current_dir_inode = target;

		uint32_t pos = strsize(current_path) - 1;
		for(uint32_t k = 0; args[k] != 0 && pos < 253; k++){
			current_path[pos++] = args[k];
		}
		current_path[pos++] = '/';
		current_path[pos] = 0;
	}

	pmm_free_block(page_phys);
}


// COMMAND CHMOD-------------------------------------------
void shell_command_chmod(char *args){
	char name[29];
	char mode;
	char change;

	uint8_t i = 0;

	for(; args[i] != ' '; i++){
		if(i >= 28){
			crlf_cursor();
			print_str((uint8_t *)"Error [size]: The name is too long.", 255, 50, 50, 255);

			return;
		}

		name[i] = args[i];
	}
	name[i] = 0;

	i++;

	if(args[i + 1] != ' ' || (args[i] != '1' && args[i] != '2' && args[i] != '3')){
		crlf_cursor();
		print_str((uint8_t *)"Mode not found. Only 1 (execute), 2 (write) and 3 (read).", 255, 50, 50, 255);
		return;
	}

	mode = args[i];

	i += 2;

	if(args[i + 1] != 0 || (args[i] != '0' && args[i] != '1')){
		crlf_cursor();
		print_str((uint8_t *)"Change not found. Only 0 (off) and 1 (on).", 255, 50, 50, 255);
		return;
	}

	change = args[i];

	int id = mik_lookup(current_dir_inode, name);

	if(id == -1){
		crlf_cursor();
		print_str((uint8_t *)"File not found or is not a file.", 255, 50, 50, 255);
		return;
	}

	int var1;
	bool var2;

	if(mode == '1') var1 = 1;
	else if(mode == '2') var1 = 2;
	else var1 = 3;

	if(change == '1') var2 = true;
	else var2 = false;

	mik_chmod(id, var1, var2);
}


// COMMAND IGNITE---------------------------------------------
void shell_command_ignite(char *args){
	char name[29];

	uint8_t i = 0;

	for(; args[i] != 0; i++){
		if(i >= 28){
			crlf_cursor();
			print_str((uint8_t *)"Error [size]: The name is too long.", 255, 50, 50, 255);

			return;
		}

		name[i] = args[i];
	} name[i] = 0;

	task_t *shell_task = current_task;

	task_t *proc = sexy_load(name);

	if(proc == NULL){
		return;
	}

	crlf_cursor();

	preempt_disable();
	proc->parent = shell_task;

	current_task->state = TASK_BLOCKED;
	preempt_enable();

	while(current_task->state == TASK_BLOCKED){
		__asm__ volatile("sti; hlt");
	}
}


// COMMAND LS----------------------------------------------
void shell_command_ls(char *args){
	(void)args;

	uint32_t cluster_phys = pmm_alloc_block();
	vmm_map_page(cluster_phys, cluster_phys, PTE_PCD | PTE_PRESENT | PTE_RW);
	uint8_t *cluster_buf = (uint8_t *)cluster_phys;

	uint32_t inodes_phys = pmm_alloc_block();
	vmm_map_page(inodes_phys, inodes_phys, PTE_PCD | PTE_PRESENT | PTE_RW);
	uint8_t *inodes_buf = (uint8_t *)inodes_phys;

	mik_read_block(mik_sb->inode_table_start, inodes_buf);
	mik_inode_t *inodes = (mik_inode_t *)inodes_buf;

	uint32_t curr_cluster = inodes[current_dir_inode].first_cluster;

	while(curr_cluster != MIK_CLUSTER_EOF){
		mik_read_block(curr_cluster, cluster_buf);

		mik_cluster_t *c = (mik_cluster_t *)cluster_buf;
		mik_dirent_t *entries = (mik_dirent_t *)c->data;

		for(uint32_t i  = 0; i < 127; i++){
			if(entries[i].inode_id != 0){
				crlf_cursor();

				uint32_t id = entries[i].inode_id;

				if(inodes[id].type == MIK_TYPE_DIR){
					print_str((uint8_t *)entries[i].name, 144, 213, 255, 255);
					put_char('/', 144, 213, 255, 255);
				} else {
					print_str((uint8_t *)entries[i].name, 255, 255, 255, 255);
				}
			}
		}
		curr_cluster = c->next_cluster;
	}
	pmm_free_block(cluster_phys);
	pmm_free_block(inodes_phys);
}


// COMMANDS LIST-------------------------------------------
Command commands_list[TOTAL_SHELL_COMMANDS] = {
	{"slava",     false, 0, shell_command_slava,     "GLORY TO THE URSS!"},
	{"clear",     false, 0, shell_command_clear,     "Clears the terminal screen."},
	{"echo",      true,  1, shell_command_echo,      "Prints text in terminal. (echo <text>)"},
	{"reboot",    false, 0, shell_command_reboot,    "Reboots the system."},
	{"audiotest", false, 0, shell_command_audiotest, "Plays a 1 second long pip."},
	{"ishere",    true,  1, shell_command_ishere,    "'Is this file/directory in this directory?' (ishere <name.ext>)"},
	{"mkdir",     true,  1, shell_command_mkdir,     "Creates a new directory. (mkdir <name>)"},
	{"touch",     true,  1, shell_command_touch,     "Creates a new file. (touch <name>)"},
	{"mv",        true,  2, shell_command_mv,        "Move things to another directories. (mv <thing_here> <destine_rout>)"},
	{"rm",        true,  2, shell_command_rm,        "Remove files and directories. (rm <f/d> <name>)"},
	{"cd",        true,  1, shell_command_cd,        "Changes the current directory. (cd <dir>)"},
	{"chmod",     true,  3, shell_command_chmod,     "Changes file permissions. (chmod <file> <perm> <state>)"},
	{"ignite",    true,  1, shell_command_ignite,    "Runs a .sexy executable. (ignite <program.sexy>)"},
	{"ls",        false, 0, shell_command_ls,        "Lists directory contents."},
	{"help",      false, 0, shell_command_help,      "Displays the list of available commands."},
	{NULL,        false, 0, NULL,                    NULL}
};
