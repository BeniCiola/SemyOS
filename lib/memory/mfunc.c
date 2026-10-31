void* kmalloc(size_t size){
	if(size == 0) return NULL;

	BlockHeader* current = heap_start;
	BlockHeader* last = NULL;

	while(current != NULL){
		if(current->is_free){
			if(current->size >= size){
				current->is_free = 0;

				return (void*)(current + 1);
			} else if(current->next != NULL && current->next->is_free){
				if((char*)current + sizeof(BlockHeader) + current->size == (char*)current->next){
					if((current->size + current->next->size + sizeof(BlockHeader)) >= size){
						current->size += sizeof(BlockHeader) + current->next->size;
						current->next = current->next->next;

						current->is_free = 0;

						return (void*)(current + 1);
					}
				}
			}
		}

		last = current;
		current = current->next;
	}

	BlockHeader* new_block;
	if(last == NULL){
		new_block = (BlockHeader*)0x00500000;
		heap_start = new_block;
	} else{
		new_block = (BlockHeader*)((char*)last + sizeof(BlockHeader) + last->size);
		last->next = new_block;
	}

	new_block->size = size;
	new_block->is_free = 0;
	new_block->next = NULL;

	return (void*)(new_block + 1);
}

void kfree(void* ptr){
	if(ptr == NULL) return;

	BlockHeader* block = ((BlockHeader*)ptr) - 1;
	block->is_free = 1;
}
