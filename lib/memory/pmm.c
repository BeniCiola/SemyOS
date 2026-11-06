#define PAGE_SIZE 4096
#define BLOCKS_PER_BYTE 8

uint32_t* memory_bitmap = NULL;
uint32_t total_blocks = 0;
uint32_t used_blocks = 0;

void pmm_init(uint32_t mem_size_bytes, uint32_t bitmap_start_addr){
    total_blocks = mem_size_bytes / PAGE_SIZE;
    memory_bitmap = (uint32_t*)bitmap_start_addr;

    for(uint32_t i = 0; i < (total_blocks / 32); i++){
        memory_bitmap[i] = 0xFFFFFFFF;
    }
}

void pmm_set_region(uint32_t base, uint32_t size, int status){
    uint32_t start_block = base / PAGE_SIZE;
    uint32_t num_blocks = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    for(uint32_t i = 0; i < num_blocks; i++){
        uint32_t block = start_block + i;

        if(status){
            memory_bitmap[block / 32] |= (1 << (block % 32));
        } else {
            memory_bitmap[block / 32] &= ~(1 << (block % 32));
        }
    }
}

uint32_t pmm_alloc_block(void){
    for(uint32_t i = 0; i < (total_blocks / 32); i++){
        if(memory_bitmap[i] != 0xFFFFFFFF){
            for(int j = 0; j < 32; j++){
                int bit = 1 << j;

                if(!(memory_bitmap[i] & bit)){
                    uint32_t block = i * 32 + j;
                    memory_bitmap[i] |= bit;

                    used_blocks++;

                    return block * PAGE_SIZE;
                }
            }
        }
    }
    return 0;
}

void pmm_free_block(uint32_t paddr){
    uint32_t block = paddr / PAGE_SIZE;
    memory_bitmap[block / 32] &= ~(1 << (block % 32));
    used_blocks--;
}
