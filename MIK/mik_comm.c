int mik_alloc_cluster(void){
	uint32_t page_phys = pmm_alloc_block();
	vmm_map_page(page_phys, page_phys, PTE_PCD | PTE_PRESENT | PTE_RW);
	uint8_t *buf = (uint8_t *)page_phys;

	mik_read_block(mik_sb->bitmap_start, buf);

	for(uint16_t byte_i = 0; byte_i <= 4095; byte_i++){
		for(uint8_t bit = 0; bit <= 7; bit++){
			if ((buf[byte_i] & (1 << bit)) == 0){
				buf[byte_i] |= (1 << bit);
				mik_write_block(mik_sb->bitmap_start, buf);

				uint32_t cluster_num = (byte_i * 8) + bit;

				for (uint16_t i = 0; i <= 4095; i++) {
    				buf[i] = 0;
				}

				mik_cluster_t *c = (mik_cluster_t *)buf;
				c->next_cluster = MIK_CLUSTER_EOF;

				mik_write_block(cluster_num, buf);

				pmm_free_block(page_phys);

				return cluster_num;
			}
		}
	}

	pmm_free_block(page_phys);

	return -1;
}

int mik_alloc_inode(void){
	uint32_t page_phys = pmm_alloc_block();
	vmm_map_page(page_phys, page_phys, PTE_PCD | PTE_PRESENT | PTE_RW);
	uint8_t *buf = (uint8_t *)page_phys;

	mik_read_block(mik_sb->inode_table_start, buf);

	mik_inode_t *inodes = (mik_inode_t *)buf;

	for(uint32_t i = 1; i < mik_sb->inode_count; i++){
		if (inodes[i].type == MIK_TYPE_FREE) {
			pmm_free_block(page_phys);
			return i;
		}
	}

	pmm_free_block(page_phys);
	return -1;
}

int mik_lookup(uint32_t dir_inode_id, const char *name){
	uint32_t page_phys = pmm_alloc_block();
	vmm_map_page(page_phys, page_phys, PTE_PCD | PTE_PRESENT | PTE_RW);
	uint8_t *buf = (uint8_t *)page_phys;

	mik_read_block(mik_sb->inode_table_start, buf);

	mik_inode_t *inodes = (mik_inode_t *)buf;

	if(inodes[dir_inode_id].type != MIK_TYPE_DIR){
		pmm_free_block(page_phys);
		return -1;
	}

	uint32_t curr_cluster = inodes[dir_inode_id].first_cluster;

	while(curr_cluster != MIK_CLUSTER_EOF){
		mik_read_block(curr_cluster, buf);

		mik_cluster_t *c = (mik_cluster_t *)buf;
		mik_dirent_t *entries = (mik_dirent_t *)c->data;

		for (uint32_t i = 0; i < 127; i++){
			if(entries[i].inode_id != 0){
				if(strcmp(entries[i].name, name)){
					uint32_t found = entries[i].inode_id;
					pmm_free_block(page_phys);
					return found;
				}
			}
		}
		curr_cluster = c->next_cluster;
	}
	pmm_free_block(page_phys);
	return -1;
}


int mik_create(uint32_t parent_inode_id, const char *name, uint8_t type, uint8_t perms){
	if(strsize(name) > 28) return -2;

	if(mik_lookup(parent_inode_id, name) != -1) return -3;

	int new_inode = mik_alloc_inode();
	if(new_inode == -1) return -4;

	int new_cluster = mik_alloc_cluster();
	if(new_cluster == -1) return -4;

	uint32_t page_phys = pmm_alloc_block();
	vmm_map_page(page_phys, page_phys, PTE_PCD | PTE_PRESENT | PTE_RW);
	uint8_t *buf = (uint8_t *)page_phys;

	mik_read_block(mik_sb->inode_table_start, buf);

	mik_inode_t *inodes = (mik_inode_t *)buf;

	inodes[new_inode].type = type;
	inodes[new_inode].perms = perms;
	inodes[new_inode].links = 1;
	inodes[new_inode].size = 0;
	inodes[new_inode].first_cluster = new_cluster;
	inodes[new_inode].parent_inode = parent_inode_id;

	mik_write_block(mik_sb->inode_table_start, buf);

	uint32_t curr_cluster = inodes[parent_inode_id].first_cluster;

	while(curr_cluster != MIK_CLUSTER_EOF){
		mik_read_block(curr_cluster, buf);

		mik_cluster_t *c = (mik_cluster_t *)buf;
		mik_dirent_t *entries = (mik_dirent_t *)c->data;

		for(uint32_t i = 0; i < 127; i++){
			if(entries[i].inode_id == 0){
				entries[i].inode_id = new_inode;

				uint32_t len = strsize(name);
				for (uint32_t k = 0; k < 28; k++) {
    				entries[i].name[k] = (k < len) ? name[k] : 0;
				}

				mik_write_block(curr_cluster, buf);

				mik_read_block(mik_sb->inode_table_start, buf);

				inodes = (mik_inode_t *)buf;
				inodes[parent_inode_id].size += sizeof(mik_dirent_t);
				mik_write_block(mik_sb->inode_table_start, buf);

				pmm_free_block(page_phys);

				return 1;
			}
		}
		curr_cluster = c->next_cluster;
	}

	pmm_free_block(page_phys);

	return -1;
}


int mik_chmod(uint32_t inode_id, int mode, bool change){
	uint32_t page_phys = pmm_alloc_block();
	vmm_map_page(page_phys, page_phys, PTE_PCD | PTE_PRESENT | PTE_RW);
	uint8_t *buf = (uint8_t *)page_phys;

	mik_read_block(mik_sb->inode_table_start, buf);
	mik_inode_t *inodes = (mik_inode_t *)buf;

	if(inodes[inode_id].type != MIK_TYPE_FILE){
		pmm_free_block(page_phys);
		return -1;
	}

	if(change){
		switch(mode){
			case 1:
				inodes[inode_id].perms |= MIK_PERM_EXEC;
				break;

			case 2:
				inodes[inode_id].perms |= MIK_PERM_WRITE;
				break;

			case 3:
				inodes[inode_id].perms |= MIK_PERM_READ;
				break;

			default:
				pmm_free_block(page_phys);
				return -2;
		}
	} else {
		switch(mode){
			case 1:
				inodes[inode_id].perms &= ~MIK_PERM_EXEC;
				break;

			case 2:
				inodes[inode_id].perms &= ~MIK_PERM_WRITE;
				break;

			case 3:
				inodes[inode_id].perms &= ~MIK_PERM_READ;
				break;

			default:
				pmm_free_block(page_phys);
				return -2;
		}

	}

	mik_write_block(mik_sb->inode_table_start, buf);

	pmm_free_block(page_phys);

	return 1;
}

/*
void mik_free_chain(uint32_t start_cluster){
	if(start_cluster == 0 || start_cluster == MIK_CLUSTER_EOF){
		return -1;
	}

	uint32_t page_phys = pmm_alloc_block();
	vmm_map_page(page_phys, page_phys, PTE_PCD | PTE_PRESENT | PTE_RW);
	uint8_t *buf = (uint8_t *)page_phys;

	mik_lookup(parent_inode_id, )

	uint32_t curr_cluster = inodes[parent_inode_id].first_cluster;

	while(curr != MIK_CLUSTER_EOF){

	}

	pmm_free_block(page_phys);
}
*/
