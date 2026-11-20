int mik_format(void){
	uint32_t page_phys = pmm_alloc_block();
	vmm_map_page(page_phys, page_phys, PTE_PCD | PTE_PRESENT | PTE_RW);
	uint8_t *buf = (uint8_t *)page_phys;

	for(uint16_t i = 0; i <= 4095; i++){
		buf[i] = 0;
	}

	mik_superblock_t *sb = (mik_superblock_t *)buf;

	sb->magic = MIK_MAGIC;
	sb->total_blocks = 2048;
	sb->bitmap_start = 1;
	sb->bitmap_blocks = 1;
	sb->inode_table_start = 2;
	sb->inode_count = 128;
	sb->data_start = 3;

	if(!mik_write_block(0, buf)){
		pmm_free_block(page_phys);
		return 0;
	}


	for(uint16_t i = 0; i <= 4095; i++){
		buf[i] = 0;
	}

	buf[0] = 0x0F;

	if(!mik_write_block(1, buf)){
		pmm_free_block(page_phys);
		return 0;
	}


	for(uint16_t i = 0; i <= 4095; i++){
		buf[i] = 0;
	}

	mik_inode_t *inodes = (mik_inode_t *)buf;

	inodes[0].type = MIK_TYPE_DIR;
	inodes[0].perms = MIK_PERM_READ | MIK_PERM_WRITE | MIK_PERM_EXEC;
	inodes[0].links = 1;
	inodes[0].size = 0;
	inodes[0].first_cluster = 3;
	inodes[0].parent_inode = 0;

	if(!mik_write_block(2, buf)){
		pmm_free_block(page_phys);
		return 0;
	}


	for(uint16_t i = 0; i <= 4095; i++){
		buf[i] = 0;
	}

	mik_cluster_t *root_data = (mik_cluster_t *)buf;

	root_data->next_cluster = MIK_CLUSTER_EOF;

	if(!mik_write_block(3, buf)){
		pmm_free_block(page_phys);
		return 0;
	}


	pmm_free_block(page_phys);
	return 1;
}


int mik_mount(void){
	if (mik_mounted) return 1;

	uint32_t page_phys = pmm_alloc_block();
	vmm_map_page(page_phys, page_phys, PTE_PCD | PTE_PRESENT | PTE_RW);
	uint8_t *buf = (uint8_t *)page_phys;

	if(!mik_read_block(0, buf)){
		sleep_millis(50);
		if(!mik_read_block(0, buf)){
			print_str((uint8_t *)"Cannot read disk. Entering in sleep mode...", 255, 10, 10, 255);
			while(1){
				__asm__ volatile("hlt");
			}
		}
	}

	mik_superblock_t *sb = (mik_superblock_t *)buf;
	if(sb->magic != MIK_MAGIC && sb->magic != 0){
		print_str((uint8_t *)"Not compatible MIK version.", 255, 10, 10, 255);
		crlf_cursor();

		kbd_ring_buffer_init(&kbd_buffer);

		print_str((uint8_t *)"Mount a compatible version? (Y/N)", 255, 255, 255, 255);
		crlf_cursor();

		while(1){
			__asm__ volatile("cli");
			if(kbd_ring_buffer_is_empty(&kbd_buffer)){
				__asm__ volatile("sti; hlt" ::: "memory");
				continue;
			}
			__asm__ volatile("sti");

			uint8_t l = kbd_ring_buffer_pop(&kbd_buffer);

			if(l >= 'A' && l <= 'Z'){
				l += 32;
			}

			if(l == 'y'){
				print_str((uint8_t *)"Mounting a compatible version...", 255, 255, 255, 255);
				crlf_cursor();

				if(!mik_format()){
					print_str((uint8_t *)"An disk error has ocurred. Entering in sleep mode.", 255, 10, 10, 255);
					while(1){
						__asm__ volatile("hlt");
					}
				}
				mik_read_block(0, buf);

				break;
			} else if(l == 'n'){
				print_str((uint8_t *)"Entering in sleep mode.", 255, 255, 255, 255);
				while(1){
					__asm__ volatile("hlt");
				}
			}
		}
	} else if(sb->magic == 0){
		print_str((uint8_t *)"MIK File System not instaled.", 255, 255, 255, 255);
		crlf_cursor();
		print_str((uint8_t *)"Installing MIKFS...", 255, 255, 255, 255);
		crlf_cursor();

		if(!mik_format()){
			print_str((uint8_t *)"An disk error has ocurred. Entering in sleep mode.", 255, 10, 10, 255);
			while(1){
				__asm__ volatile("hlt");
			}
		}
		mik_read_block(0, buf);
	}

	mik_sb = sb;
	mik_mounted = 1;

	return 1;
}
