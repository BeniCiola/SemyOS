uint32_t current_heap_virtual_end = 0x00500000;

void* kmalloc(size_t size){
	if(size == 0) return NULL;

	BlockHeader* current = heap_start;
	BlockHeader* last = NULL;

	while(current != NULL){
		if(current->is_free && current->size >= size){
			current->is_free = 0;
			return (void*)(current + 1);
		}
		last = current;
		current = current->next;
	}

	size_t needed_space = size + sizeof(BlockHeader);
	uint32_t pages_needed = (needed_space + PAGE_SIZE - 1) / PAGE_SIZE;

	BlockHeader* new_block;

	if(last == NULL){
		new_block = (BlockHeader*)current_heap_virtual_end;
		heap_start = new_block;
	} else {
		new_block = (BlockHeader*)((char*)last + sizeof(BlockHeader) + last->size);
	}

	for(uint32_t p = 0; p < pages_needed; p++){
		uint32_t virt_addr = (uint32_t)new_block + (p * PAGE_SIZE);

		if(virt_addr >= current_heap_virtual_end){
			uint32_t phys_frame = pmm_alloc_block();

			if(!phys_frame) return NULL;

			vmm_map_page(virt_addr, phys_frame, 0);

			current_heap_virtual_end += PAGE_SIZE;
		}
	}

	new_block->size = size;
	new_block->is_free = 0;
	new_block->next = NULL;

	if(last != NULL){
		last->next = new_block;
	}

	return (void*)(new_block + 1);
}

void kfree(void* ptr){
    if(ptr == NULL) return;

    BlockHeader* block = ((BlockHeader*)ptr) - 1;
    block->is_free = 1;
}

void* memcpy(void *dest, const void *src, size_t n){
	uint8_t *d = (uint8_t *)dest;
	const uint8_t *s = (const uint8_t *)src;

	while(n >= 4){
		*(uint32_t *)d = *(const uint32_t *)s;
		d += 4; s += 4; n -= 4;
	}

	while(n > 0){
		*d++ = *s++; n--;
	}

	return dest;
}
