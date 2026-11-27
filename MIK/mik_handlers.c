int mik_write(uint32_t inode_id, const void *data, uint32_t size){
	uint32_t page_phys = pmm_alloc_block();
	vmm_map_page(page_phys, page_phys, PTE_PCD | PTE_PRESENT | PTE_RW);
	uint8_t *buf = (uint8_t *)page_phys;

	mik_inode_t inode;
	mik_read_inode(inode_id, &inode, buf);

	if(inode.type != MIK_TYPE_FILE){
		pmm_free_block(page_phys);
		return -1;
	}

	if((inode.perms & MIK_PERM_WRITE) == 0){
		pmm_free_block(page_phys);
		return -2;
	}

	uint32_t curr_cluster = inode.first_cluster;

	uint32_t written = 0;
	const uint8_t *src = (const uint8_t *)data;

	while(written < size){
		mik_read_block(curr_cluster, buf);
		mik_cluster_t *c = (mik_cluster_t *)buf;

		for(uint32_t i = 0; i < 4092 && written < size; i++){
			c->data[i] = src[written];
			written++;
		}

		if(written < size && c->next_cluster == MIK_CLUSTER_EOF){
			int next = mik_alloc_cluster();
			if(next == -1){
				mik_write_block(curr_cluster, buf);

				inode.size = written;

				mik_write_inode(inode_id, &inode, buf);

				pmm_free_block(page_phys);
				return -3;
			}
			c->next_cluster = next;
		}

		mik_write_block(curr_cluster, buf);

		curr_cluster = c->next_cluster;
	}

	inode.size = written;

	mik_write_inode(inode_id, &inode, buf);

	pmm_free_block(page_phys);
	return written;
}

int mik_read(uint32_t inode_id, void *buffer, uint32_t max_size){
	uint32_t page_phys = pmm_alloc_block();
	vmm_map_page(page_phys, page_phys, PTE_PCD | PTE_PRESENT | PTE_RW);
	uint8_t *buf = (uint8_t *)page_phys;

	mik_inode_t inode;
	mik_read_inode(inode_id, &inode, buf);

	if(inode.type != MIK_TYPE_FILE){
		pmm_free_block(page_phys);
		return -1;
	}
	if((inode.perms & MIK_PERM_READ) == 0){
		pmm_free_block(page_phys);
		return -2;
	}

	uint32_t curr_cluster = inode.first_cluster;
	uint32_t to_read = (inode.size < max_size) ? inode.size : max_size;

	uint8_t *dst = (uint8_t *)buffer;
	uint32_t read_bytes = 0;

	while(read_bytes < to_read && curr_cluster != MIK_CLUSTER_EOF){
		mik_read_block(curr_cluster, buf);
		mik_cluster_t *c = (mik_cluster_t *)buf;

		for(uint32_t i = 0; i < 4092 && read_bytes < to_read; i++){
			dst[read_bytes] = c->data[i];
			read_bytes++;
		}
		curr_cluster = c->next_cluster;
	}

	pmm_free_block(page_phys);
	return read_bytes;
}
