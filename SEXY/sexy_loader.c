task_t *sexy_load(const char *name){
	int id = mik_lookup(current_dir_inode, name);

	if(id < 0){
		crlf_cursor();
		print_str((uint8_t *)"The file doesn't exist.", 255, 50, 50, 255);
		return NULL;
	}

	uint32_t page_phys = pmm_alloc_block();
	vmm_map_page(page_phys, page_phys, PTE_PCD | PTE_PRESENT | PTE_RW);
	uint8_t *buf = (uint8_t *)page_phys;

	mik_inode_t inode;
	mik_read_inode(id, &inode, buf);

	if(inode.type != MIK_TYPE_FILE){
		crlf_cursor();
		print_str((uint8_t *)"The file isn't a file.", 255, 50, 50, 255);

		pmm_free_block(page_phys);

		return NULL;
	}

	if((inode.perms & MIK_PERM_EXEC) == 0){
		crlf_cursor();
		print_str((uint8_t *)"The file doesn't has execute permission.", 255, 50, 50, 255);

		pmm_free_block(page_phys);

		return NULL;
	}

	if(inode.size < sizeof(sexy_header_t)){
		crlf_cursor();
		print_str((uint8_t *)"Mm... here don't fit a sexy header.", 255, 50, 50, 255);

		pmm_free_block(page_phys);

		return NULL;
	}

	uint32_t file_size = inode.size;

	pmm_free_block(page_phys);

	uint8_t *file_buf = kmalloc(file_size);

	mik_read(id, file_buf, file_size);

	sexy_header_t *hdr = (sexy_header_t *)file_buf;

	if(hdr->magic != SEXY_MAGIC){
		crlf_cursor();
		print_str((uint8_t *)"The magic number doesn't coincide.", 255, 50, 50, 255);

		kfree(file_buf);

		return NULL;
	}

	uint32_t total = hdr->code_size + hdr->bss_size;
	uint32_t pages = (total + 4095) / 4096;

	for(uint32_t p = 0; p < pages; p++){
		uint32_t phys = pmm_alloc_block();

		if(!phys){

			kfree(file_buf);
			pmm_free_block(phys);

			return NULL;
		}

		vmm_map_page(hdr->load_addr + (p * 4096), phys, PTE_PRESENT | PTE_RW);
	}

	memcpy((void *)hdr->load_addr, file_buf + sizeof(sexy_header_t), hdr->code_size);

	uint8_t *bss = (uint8_t *)hdr->load_addr + hdr->code_size;
	for(uint32_t i = 0; i < hdr->bss_size; i++){
		bss[i] = 0;
	}

	uint32_t entry = hdr->entry_point;

	kfree(file_buf);

	return task_create((void (*)(void))entry);
}
