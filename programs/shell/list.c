typedef void (*CommandFunc)(char *args);

typedef struct {
	char *name;
	bool args;
	uint8_t args_num;
	CommandFunc function;
} Command;


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


// COMMAND AUDIOTEST---------------------------------------
void shell_command_audiotest(char *args){
	static uint16_t tone[48];
	for(int i = 0; i < 48; i++){
    	tone[i] = (i < 24) ? 4000 : (uint16_t)-4000;
	}
	hda_play_sound(tone, 48, 1000);
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


// COMMAND CD----------------------------------------------
void shell_command_cd(char *args){
	uint32_t page_phys = pmm_alloc_block();
	vmm_map_page(page_phys, page_phys, PTE_PCD | PTE_PRESENT | PTE_RW);
	uint8_t *buf = (uint8_t *)page_phys;

	mik_read_block(mik_sb->inode_table_start, buf);
	mik_inode_t *inodes = (mik_inode_t *)buf;

	if(strcmp(args, "..")){
		current_dir_inode = inodes[current_dir_inode].parent_inode;
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
	if(target == -1){
		crlf_cursor();
		print_str((uint8_t *)"Directory not found.", 255, 55, 55, 255);
	} else if(inodes[target].type != MIK_TYPE_DIR){
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


// COMMAND TOUCH-------------------------------------------
void shell_command_touch(char *args){
	mik_create(current_dir_inode, args, MIK_TYPE_FILE, MIK_PERM_WRITE | MIK_PERM_READ);
}


// COMMAND CHMOD-------------------------------------------
void shell_command_chmod(char *args){
	char name[28];
	char mode;
	char change;

	uint8_t i = 0;

	for(; args[i] != ' '; i++){
		if(i > 27){
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
Command commands_list[] = {
	{"clear", false, 0, shell_command_clear},
	{"echo", true, 1, shell_command_echo},
	{"reboot", false, 0, shell_command_reboot},
	{"audiotest", false, 0, shell_command_audiotest},
	{"ishere", true, 1, shell_command_ishere},
	{"mkdir", true, 1, shell_command_mkdir},
	{"touch", true, 1, shell_command_touch},
	{"cd", true, 1, shell_command_cd},
	{"chmod", true, 3, shell_command_chmod},
	{"ls", false, 0, shell_command_ls},
	{NULL, false, 0, NULL}
};
