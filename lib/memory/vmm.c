#define PTE_USER 0x04

__attribute__((aligned(4096))) uint32_t page_directory[1024];

void vmm_init(void) {
    for(int i = 0; i < 1024; i++) {
        page_directory[i] = 0x00000002;
    }

	for(uint32_t t = 0; t < 1024; t++){
		uint32_t pt_phys = pmm_alloc_block();
		uint32_t *page_table = (uint32_t *)pt_phys;

		for(int i = 0; i < 1024; i++){
			uint32_t phys_addr = (t * 0x400000) + (i * 0x1000);
			page_table[i] = phys_addr | PTE_PRESENT | PTE_RW;
		}

		page_directory[t] = pt_phys | PTE_PRESENT | PTE_RW;
	}

	__asm__ volatile("mov %0, %%cr3" : : "r"((uint32_t)page_directory));

	uint32_t cr0;

	__asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
	cr0 |= (1 << 31);
	__asm__ volatile("mov %0, %%cr0" : : "r"(cr0));
}

void vmm_map_page(uint32_t virt, uint32_t phys, uint32_t flags){
	uint32_t pd_index = virt >> 22;
	uint32_t pt_index = (virt >> 12) & 0x03FF;

	uint32_t* page_table;

	if(!(page_directory[pd_index] & PTE_PRESENT)){
		uint32_t pt_phys = pmm_alloc_block();

		page_directory[pd_index] = pt_phys | PTE_PRESENT | PTE_RW | flags;

		page_table = (uint32_t*)pt_phys;

		for(int i = 0; i < 1024; i++){
			page_table[i] = 0;
		}
	} else {
		uint32_t pt_phys = page_directory[pd_index] & 0xFFFFF000;
		page_table = (uint32_t*)pt_phys;
	}

	page_table[pt_index] = (phys & 0xFFFFF000) | PTE_PRESENT | PTE_RW | flags;

	__asm__ volatile("invlpg (%0)" : : "r" (virt) : "memory");
}
